//! The [`Backend`] on the `wgpu` crate. Thin on purpose: every choice is made in `gx`/`keys`; this file turns them into
//! wgpu calls.

use std::collections::HashMap;
use std::ffi::c_void;
use std::num::NonZeroU64;
use std::ptr::NonNull;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::Arc;

use wgpu::*;

use crate::abi::*;
use crate::gx::{self, BlitDraw, BlitTarget, DrawCall, ReadBack, RingKind, StartConfig, StartError, SwapCaps};
use crate::gx::{INDEX_RING_SIZE, UNIFORM_BINDING, UNIFORM_RING_SIZE, VERTEX_RING_SIZE};
use crate::keys::{self, BlitMode, SamplerDesc};
use crate::plat::gx_log;
use crate::shaders;
use crate::surface::SurfaceId;

fn block_on<F: std::future::Future>(future: F) -> F::Output {
    let mut future = std::pin::pin!(future);
    let mut context = std::task::Context::from_waker(std::task::Waker::noop());
    loop {
        if let std::task::Poll::Ready(value) = future.as_mut().poll(&mut context) {
            return value;
        }
        std::thread::yield_now();
    }
}

/// wgpu-native's `get_base_device_limits_from_adapter_limits`: the default limits at the adapter's texture sizes, falling
/// back to the downlevel sets.
fn base_limits(adapter: &Limits) -> Limits {
    let default = Limits::default();
    let dim_1d = adapter.max_texture_dimension_1d.min(default.max_texture_dimension_1d);
    let dim_2d = adapter.max_texture_dimension_2d.min(default.max_texture_dimension_2d);
    let dim_3d = adapter.max_texture_dimension_3d.min(default.max_texture_dimension_3d);
    let with = |base: Limits| Limits {
        max_texture_dimension_1d: dim_1d,
        max_texture_dimension_2d: dim_2d,
        max_texture_dimension_3d: dim_3d,
        ..base
    };
    let candidate = with(Limits::default());
    if candidate.check_limits(adapter) {
        return candidate;
    }
    let candidate = with(Limits::downlevel_defaults());
    if candidate.check_limits(adapter) {
        return candidate;
    }
    with(Limits::downlevel_webgl2_defaults())
}

fn backend_number(backend: Backend) -> u32 {
    match backend {
        Backend::Empty => 1,
        Backend::BrowserWebGpu => 2,
        Backend::Dx12 => 4,
        Backend::Metal => 5,
        Backend::Vulkan => 6,
        Backend::Gl => 7,
    }
}

fn device_type_number(device_type: DeviceType) -> u32 {
    match device_type {
        DeviceType::DiscreteGpu => 1,
        DeviceType::IntegratedGpu => 2,
        DeviceType::Cpu => 3,
        DeviceType::VirtualGpu | DeviceType::Other => 4,
    }
}

const WHITE: [u8; 4] = [255, 255, 255, 255];

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
enum PipelineId {
    Fixed(u32),
    Blit(BlitMode, TextureFormat, bool),
}

#[derive(Default)]
struct GpuSurface {
    texture: Option<Texture>,
    view: Option<TextureView>,
    target_view: Option<TextureView>,
    groups: HashMap<u32, BindGroup>,
    swap: Option<wgpu::Surface<'static>>,
    frame: Option<SurfaceTexture>,
    frame_view: Option<TextureView>,
}

struct Core {
    instance: Instance,
    adapter: Adapter,
    device: Device,
    queue: Queue,
    vertex_ring: Buffer,
    index_ring: Buffer,
    uniform_ring: Buffer,
    layout: BindGroupLayout,
    pipeline_layout: PipelineLayout,
    shaders: [ShaderModule; 2],
    blit_shader: ShaderModule,
    pipelines: HashMap<PipelineId, RenderPipeline>,
    samplers: HashMap<u32, Sampler>,
    white_view: TextureView,
    white_groups: HashMap<u32, BindGroup>,
}

#[derive(Default)]
pub struct WgpuBackend {
    core: Option<Core>,
    surfaces: Vec<GpuSurface>,
    encoder: Option<CommandEncoder>,
    pass: Option<RenderPass<'static>>,
    pass_pipeline: Option<PipelineId>,
}

impl WgpuBackend {
    fn core(&self) -> &Core {
        self.core.as_ref().expect("wgpu started")
    }

    fn slot(&mut self, id: SurfaceId) -> &mut GpuSurface {
        if self.surfaces.len() <= id {
            self.surfaces.resize_with(id + 1, GpuSurface::default);
        }
        &mut self.surfaces[id]
    }

    fn encoder(&mut self) -> &mut CommandEncoder {
        if self.encoder.is_none() {
            let encoder = self.core().device.create_command_encoder(&CommandEncoderDescriptor::default());
            self.encoder = Some(encoder);
        }
        self.encoder.as_mut().unwrap()
    }

    fn group(&mut self, texture: Option<SurfaceId>, sampler_key: u32) -> BindGroup {
        let mut view = None;
        if let Some(id) = texture {
            if let Some(found) = self.slot(id).groups.get(&sampler_key) {
                return found.clone();
            }
            view = self.slot(id).view.clone();
        } else if let Some(found) = self.core().white_groups.get(&sampler_key) {
            return found.clone();
        }
        let sampler = self.sampler(sampler_key);
        let core = self.core();
        let view = view.unwrap_or_else(|| core.white_view.clone());
        let group = core.device.create_bind_group(&BindGroupDescriptor {
            label: None,
            layout: &core.layout,
            entries: &[
                BindGroupEntry {
                    binding: 0,
                    resource: BindingResource::Buffer(BufferBinding {
                        buffer: &core.uniform_ring,
                        offset: 0,
                        size: NonZeroU64::new(UNIFORM_BINDING),
                    }),
                },
                BindGroupEntry { binding: 1, resource: BindingResource::TextureView(&view) },
                BindGroupEntry { binding: 2, resource: BindingResource::Sampler(&sampler) },
            ],
        });
        match texture {
            Some(id) => {
                self.slot(id).groups.insert(sampler_key, group.clone());
            }
            None => {
                self.core.as_mut().unwrap().white_groups.insert(sampler_key, group.clone());
            }
        }
        group
    }

    fn sampler(&mut self, key: u32) -> Sampler {
        if let Some(found) = self.core().samplers.get(&key) {
            return found.clone();
        }
        let SamplerDesc { address, min_filter, mag_filter, mip_filter, lod_min, lod_max, anisotropy } =
            keys::sampler_desc(key);
        let sampler = self.core().device.create_sampler(&SamplerDescriptor {
            label: None,
            address_mode_u: address,
            address_mode_v: address,
            address_mode_w: address,
            mag_filter,
            min_filter,
            mipmap_filter: mip_filter,
            lod_min_clamp: lod_min,
            lod_max_clamp: lod_max,
            compare: None,
            anisotropy_clamp: anisotropy,
            border_color: None,
        });
        self.core.as_mut().unwrap().samplers.insert(key, sampler.clone());
        sampler
    }

    fn pipeline(&mut self, id: PipelineId) -> RenderPipeline {
        if let Some(found) = self.core().pipelines.get(&id) {
            return found.clone();
        }
        let pipeline = match id {
            PipelineId::Fixed(key) => self.create_fixed_pipeline(key),
            PipelineId::Blit(mode, format, depth) => self.create_blit_pipeline(mode, format, depth),
        };
        self.core.as_mut().unwrap().pipelines.insert(id, pipeline.clone());
        pipeline
    }

    fn create_fixed_pipeline(&self, key: u32) -> RenderPipeline {
        let core = self.core();
        let desc = keys::pipeline_desc(key);
        let attributes = [
            VertexAttribute { format: VertexFormat::Float32x3, offset: 0, shader_location: 0 },
            VertexAttribute { format: VertexFormat::Float32x3, offset: 12, shader_location: 1 },
            VertexAttribute { format: VertexFormat::Unorm8x4, offset: 24, shader_location: 2 }, // the ARGB dword (swizzled)
            VertexAttribute { format: VertexFormat::Float32x2, offset: 28, shader_location: 3 },
        ];
        let buffers =
            [VertexBufferLayout { array_stride: 36, step_mode: VertexStepMode::Vertex, attributes: &attributes }];
        let module = &core.shaders[usize::from(desc.flat)];
        let targets = [Some(ColorTargetState {
            format: TextureFormat::Bgra8Unorm,
            blend: desc.blend,
            write_mask: ColorWrites::ALL,
        })];
        core.device.create_render_pipeline(&RenderPipelineDescriptor {
            label: None,
            layout: Some(&core.pipeline_layout),
            vertex: VertexState {
                module,
                entry_point: Some("vertex_main"),
                compilation_options: Default::default(),
                buffers: &buffers,
            },
            // Direct3D culls by screen winding: CCW culls counter-clockwise triangles, CW clockwise ones
            primitive: PrimitiveState {
                topology: desc.topology,
                strip_index_format: None,
                front_face: FrontFace::Ccw,
                cull_mode: desc.cull,
                unclipped_depth: false,
                polygon_mode: PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: desc.depth.then_some(DepthStencilState {
                format: TextureFormat::Depth32Float,
                depth_write_enabled: desc.z_write,
                depth_compare: desc.z_compare,
                stencil: StencilState::default(),
                bias: DepthBiasState::default(),
            }),
            multisample: MultisampleState::default(),
            fragment: Some(FragmentState {
                module,
                entry_point: Some("fragment_main"),
                compilation_options: Default::default(),
                targets: &targets,
            }),
            multiview: None,
            cache: None,
        })
    }

    fn create_blit_pipeline(&self, mode: BlitMode, format: TextureFormat, depth: bool) -> RenderPipeline {
        let core = self.core();
        let write_mask = if mode == BlitMode::Depth { ColorWrites::empty() } else { ColorWrites::ALL };
        let targets = [Some(ColorTargetState { format, blend: keys::blit_blend(mode), write_mask })];
        core.device.create_render_pipeline(&RenderPipelineDescriptor {
            label: None,
            layout: Some(&core.pipeline_layout),
            vertex: VertexState {
                module: &core.blit_shader,
                entry_point: Some("vertex_main"),
                compilation_options: Default::default(),
                buffers: &[],
            },
            primitive: PrimitiveState::default(),
            depth_stencil: depth.then_some(DepthStencilState {
                format: TextureFormat::Depth32Float,
                depth_write_enabled: mode == BlitMode::Depth,
                depth_compare: CompareFunction::Always,
                stencil: StencilState::default(),
                bias: DepthBiasState::default(),
            }),
            multisample: MultisampleState::default(),
            fragment: Some(FragmentState {
                module: &core.blit_shader,
                entry_point: Some("fragment_main"),
                compilation_options: Default::default(),
                targets: &targets,
            }),
            multiview: None,
            cache: None,
        })
    }

    fn set_pipeline(&mut self, id: PipelineId) {
        if self.pass_pipeline == Some(id) {
            return;
        }
        let pipeline = self.pipeline(id);
        self.pass.as_mut().expect("an open pass").set_pipeline(&pipeline);
        self.pass_pipeline = Some(id);
    }
}

fn create_shader(device: &Device, source: &str) -> ShaderModule {
    device.create_shader_module(ShaderModuleDescriptor { label: None, source: ShaderSource::Wgsl(source.into()) })
}

fn create_ring(device: &Device, size: u64, usage: BufferUsages) -> Buffer {
    device.create_buffer(&BufferDescriptor {
        label: None,
        size,
        usage: usage | BufferUsages::COPY_DST,
        mapped_at_creation: false,
    })
}

impl crate::gx::Backend for WgpuBackend {
    fn start(&mut self, config: &StartConfig) -> Result<gx::AdapterInfo, StartError> {
        let instance = Instance::new(&InstanceDescriptor {
            backends: config.backends,
            flags: config.flags,
            backend_options: BackendOptions {
                gl: GlBackendOptions::default(),
                dx12: Dx12BackendOptions {
                    shader_compiler: if config.dxc { Dx12Compiler::StaticDxc } else { Dx12Compiler::Fxc },
                },
            },
        });
        let mut adapter = None;
        for fallback in 0..2 {
            // Windows: DX12 (WARP without a GPU), else Vulkan; Linux: Vulkan; macOS: Metal
            adapter = block_on(instance.request_adapter(&RequestAdapterOptions {
                power_preference: PowerPreference::HighPerformance,
                force_fallback_adapter: fallback != 0 || config.force_fallback,
                compatible_surface: None,
            }));
            if adapter.is_some() {
                break;
            }
        }
        let adapter = adapter.ok_or(StartError::NoAdapter)?;
        let limits = base_limits(&adapter.limits());
        let (device, queue) = block_on(adapter.request_device(
            &DeviceDescriptor {
                label: None,
                required_features: Features::empty(),
                required_limits: limits,
                memory_hints: MemoryHints::default(),
            },
            None,
        ))
        .map_err(|_| StartError::NoDevice)?;
        device.on_uncaptured_error(Box::new(|error| {
            // WGPUErrorType's numbering; the default handler panics, the C++ logged
            let kind = match &error {
                wgpu::Error::OutOfMemory { .. } => 3,
                wgpu::Error::Validation { .. } => 2,
                wgpu::Error::Internal { .. } => 4,
            };
            gx_log(&format!("GX: wgpu error {kind}: {error}"));
        }));
        let info = adapter.get_info();

        let vertex_ring = create_ring(&device, VERTEX_RING_SIZE, BufferUsages::VERTEX);
        let index_ring = create_ring(&device, INDEX_RING_SIZE, BufferUsages::INDEX);
        let uniform_ring = create_ring(&device, UNIFORM_RING_SIZE, BufferUsages::UNIFORM);
        let layout = device.create_bind_group_layout(&BindGroupLayoutDescriptor {
            label: None,
            entries: &[
                BindGroupLayoutEntry {
                    binding: 0,
                    visibility: ShaderStages::VERTEX | ShaderStages::FRAGMENT,
                    ty: BindingType::Buffer {
                        ty: BufferBindingType::Uniform,
                        has_dynamic_offset: true,
                        min_binding_size: None,
                    },
                    count: None,
                },
                BindGroupLayoutEntry {
                    binding: 1,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Texture {
                        sample_type: TextureSampleType::Float { filterable: true },
                        view_dimension: TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                BindGroupLayoutEntry {
                    binding: 2,
                    visibility: ShaderStages::FRAGMENT,
                    ty: BindingType::Sampler(SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });
        let pipeline_layout = device.create_pipeline_layout(&PipelineLayoutDescriptor {
            label: None,
            bind_group_layouts: &[&layout],
            push_constant_ranges: &[],
        });
        let shaders = [
            create_shader(&device, &shaders::fixed_function_shader(false)),
            create_shader(&device, &shaders::fixed_function_shader(true)),
        ];
        let blit_shader = create_shader(&device, shaders::BLIT_SHADER);
        let white = device.create_texture(&TextureDescriptor {
            label: None,
            size: Extent3d { width: 1, height: 1, depth_or_array_layers: 1 },
            mip_level_count: 1,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format: TextureFormat::Bgra8Unorm,
            usage: TextureUsages::TEXTURE_BINDING | TextureUsages::COPY_DST,
            view_formats: &[],
        });
        let white_view = white.create_view(&TextureViewDescriptor::default());
        queue.write_texture(
            TexelCopyTextureInfo { texture: &white, mip_level: 0, origin: Origin3d::ZERO, aspect: TextureAspect::All },
            &WHITE,
            TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(4), rows_per_image: Some(1) },
            Extent3d { width: 1, height: 1, depth_or_array_layers: 1 },
        );
        self.core = Some(Core {
            instance,
            adapter,
            device,
            queue,
            vertex_ring,
            index_ring,
            uniform_ring,
            layout,
            pipeline_layout,
            shaders,
            blit_shader,
            pipelines: HashMap::new(),
            samplers: HashMap::new(),
            white_view,
            white_groups: HashMap::new(),
        });
        Ok(gx::AdapterInfo {
            name: info.name.clone(),
            vendor_id: info.vendor,
            device_id: info.device,
            backend: backend_number(info.backend),
            device_type: device_type_number(info.device_type),
        })
    }

    fn create_texture(&mut self, id: SurfaceId, width: u32, height: u32, mip_levels: u32, is_z: bool) -> bool {
        let (format, usage) = if is_z {
            (TextureFormat::Depth32Float, TextureUsages::RENDER_ATTACHMENT)
        } else {
            (
                TextureFormat::Bgra8Unorm,
                TextureUsages::TEXTURE_BINDING
                    | TextureUsages::COPY_DST
                    | TextureUsages::COPY_SRC
                    | TextureUsages::RENDER_ATTACHMENT,
            )
        };
        let texture = self.core().device.create_texture(&TextureDescriptor {
            label: None,
            size: Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: mip_levels,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format,
            usage,
            view_formats: &[],
        });
        let view = texture.create_view(&TextureViewDescriptor::default());
        let target_view = texture.create_view(&TextureViewDescriptor {
            label: None,
            format: Some(format),
            dimension: Some(TextureViewDimension::D2),
            usage: None,
            aspect: TextureAspect::All,
            base_mip_level: 0,
            mip_level_count: Some(1),
            base_array_layer: 0,
            array_layer_count: Some(1),
        });
        let slot = self.slot(id);
        slot.texture = Some(texture);
        slot.view = Some(view);
        slot.target_view = Some(target_view);
        true
    }

    fn release_surface(&mut self, id: SurfaceId) {
        if let Some(slot) = self.surfaces.get_mut(id) {
            *slot = GpuSurface::default();
        }
    }

    fn write_ring(&mut self, ring: RingKind, offset: u64, data: &[u8]) {
        let core = self.core();
        let buffer = match ring {
            RingKind::Vertex => &core.vertex_ring,
            RingKind::Index => &core.index_ring,
            RingKind::Uniform => &core.uniform_ring,
        };
        core.queue.write_buffer(buffer, offset, data);
    }

    fn upload_level(&mut self, top: SurfaceId, mip: u32, width: u32, height: u32, bgra: &[u8]) {
        let Some(texture) = self.slot(top).texture.clone() else { return };
        self.core().queue.write_texture(
            TexelCopyTextureInfo {
                texture: &texture,
                mip_level: mip,
                origin: Origin3d::ZERO,
                aspect: TextureAspect::All,
            },
            bgra,
            TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(width * 4), rows_per_image: Some(height) },
            Extent3d { width, height, depth_or_array_layers: 1 },
        );
    }

    fn read_texture(&mut self, id: SurfaceId, width: u32, height: u32) -> ReadBack {
        let Some(texture) = self.slot(id).texture.clone() else { return ReadBack::Failed };
        let core = self.core();
        let row = (width * 4 + 255) & !255;
        let size = u64::from(row) * u64::from(height);
        let buffer = core.device.create_buffer(&BufferDescriptor {
            label: None,
            size,
            usage: BufferUsages::MAP_READ | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let mut encoder = core.device.create_command_encoder(&CommandEncoderDescriptor::default());
        encoder.copy_texture_to_buffer(
            TexelCopyTextureInfo {
                texture: &texture,
                mip_level: 0,
                origin: Origin3d::ZERO,
                aspect: TextureAspect::All,
            },
            TexelCopyBufferInfo {
                buffer: &buffer,
                layout: TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(row), rows_per_image: Some(height) },
            },
            Extent3d { width, height, depth_or_array_layers: 1 },
        );
        core.queue.submit(Some(encoder.finish()));
        let done = Arc::new(AtomicI32::new(0));
        let signal = done.clone();
        buffer.slice(..).map_async(MapMode::Read, move |result| {
            signal.store(if result.is_ok() { 1 } else { -1 }, Ordering::SeqCst);
        });
        // poll without a blocking wait (a blocking device poll never returned in an x64 session, wgpu-native v29 on
        // DX12/WARP): give the copy up to 10 s
        let start = ee_plat_core::time::PlatTimeMilliseconds64();
        let mut spin = 0u64;
        while done.load(Ordering::SeqCst) == 0 {
            core.device.poll(Maintain::Poll);
            core.instance.poll_all(false);
            if done.load(Ordering::SeqCst) != 0 {
                break;
            }
            if ee_plat_core::time::PlatTimeMilliseconds64() - start > 10000 {
                return ReadBack::TimedOut;
            }
            if spin > 64 {
                ee_plat_core::time::PlatSleep(if spin > 1000 { 1 } else { 0 });
            }
            spin += 1;
        }
        if done.load(Ordering::SeqCst) < 0 {
            return ReadBack::Failed;
        }
        let mut rows = vec![0u8; width as usize * height as usize * 4];
        {
            let mapped = buffer.slice(..).get_mapped_range();
            for y in 0..height as usize {
                rows[y * width as usize * 4..][..width as usize * 4]
                    .copy_from_slice(&mapped[y * row as usize..][..width as usize * 4]);
            }
        }
        buffer.unmap();
        ReadBack::Pixels(rows)
    }

    fn begin_pass(&mut self, target: SurfaceId, depth: Option<SurfaceId>) {
        let target_view = self.slot(target).target_view.clone().expect("the target has a texture");
        let depth_view = depth.and_then(|d| self.slot(d).target_view.clone());
        let encoder = self.encoder();
        let pass = encoder
            .begin_render_pass(&RenderPassDescriptor {
                label: None,
                color_attachments: &[Some(RenderPassColorAttachment {
                    view: &target_view,
                    resolve_target: None,
                    ops: Operations { load: LoadOp::Load, store: StoreOp::Store },
                })],
                depth_stencil_attachment: depth_view.as_ref().map(|view| RenderPassDepthStencilAttachment {
                    view,
                    depth_ops: Some(Operations { load: LoadOp::Load, store: StoreOp::Store }),
                    stencil_ops: None,
                }),
                timestamp_writes: None,
                occlusion_query_set: None,
            })
            .forget_lifetime();
        self.pass = Some(pass);
        self.pass_pipeline = None;
    }

    fn end_pass(&mut self) {
        self.pass = None;
        self.pass_pipeline = None;
    }

    fn submit(&mut self) {
        self.pass = None;
        self.pass_pipeline = None;
        if let Some(encoder) = self.encoder.take() {
            self.core().queue.submit(Some(encoder.finish()));
        }
    }

    fn draw_blit(&mut self, draw: &BlitDraw) {
        let format = match draw.format {
            BlitTarget::Surface => TextureFormat::Bgra8Unorm,
            BlitTarget::Swap(format) => format,
        };
        let group = self.group(draw.source, draw.sampler_key);
        self.set_pipeline(PipelineId::Blit(draw.mode, format, draw.depth));
        let pass = self.pass.as_mut().expect("an open pass");
        pass.set_viewport(0.0, 0.0, draw.width as f32, draw.height as f32, 0.0, 1.0);
        pass.set_scissor_rect(0, 0, draw.width, draw.height);
        pass.set_bind_group(0, &group, &[draw.uniform_offset]);
        pass.draw(0..6, 0..1);
    }

    fn draw(&mut self, draw: &DrawCall) {
        let group = self.group(draw.texture, draw.sampler_key);
        self.set_pipeline(PipelineId::Fixed(draw.pipeline_key));
        let core = self.core.as_ref().expect("wgpu started");
        let pass = self.pass.as_mut().expect("an open pass");
        let [x, y, w, h, min_z, max_z] = draw.viewport;
        pass.set_viewport(x, y, w, h, min_z, max_z);
        pass.set_scissor_rect(0, 0, draw.scissor.0, draw.scissor.1);
        pass.set_bind_group(0, &group, &[draw.uniform_offset]);
        pass.set_vertex_buffer(0, core.vertex_ring.slice(draw.vertex_offset..draw.vertex_offset + draw.vertex_bytes));
        if let Some((offset, bytes, count)) = draw.index {
            pass.set_index_buffer(core.index_ring.slice(offset..offset + bytes), IndexFormat::Uint16);
            pass.draw_indexed(0..count, 0, 0..1);
        } else {
            pass.draw(0..draw.vertex_count, 0..1);
        }
    }

    fn swap_create(&mut self, primary: SurfaceId, native: &NativeWindow) -> bool {
        let target = match surface_target(native) {
            Some(target) => target,
            None => return false,
        };
        // SAFETY: the native handles stay valid while the primary surface (the swap chain's owner) lives
        let surface = unsafe { self.core().instance.create_surface_unsafe(target) };
        match surface {
            Ok(surface) => {
                self.slot(primary).swap = Some(surface);
                true
            }
            Err(_) => false,
        }
    }

    fn swap_caps(&mut self, primary: SurfaceId) -> SwapCaps {
        let adapter = self.core().adapter.clone();
        let surface = self.slot(primary).swap.as_ref().expect("a swap chain");
        let caps = surface.get_capabilities(&adapter);
        SwapCaps { formats: caps.formats, present_modes: caps.present_modes }
    }

    fn swap_configure(
        &mut self,
        primary: SurfaceId,
        format: TextureFormat,
        width: u32,
        height: u32,
        mode: PresentMode,
    ) {
        let device = self.core().device.clone();
        let surface = self.slot(primary).swap.as_ref().expect("a swap chain");
        surface.configure(
            &device,
            &SurfaceConfiguration {
                usage: TextureUsages::RENDER_ATTACHMENT,
                format,
                width,
                height,
                present_mode: mode,
                desired_maximum_frame_latency: 2,
                alpha_mode: CompositeAlphaMode::Auto,
                view_formats: Vec::new(),
            },
        );
    }

    fn swap_acquire(&mut self, primary: SurfaceId) -> bool {
        let slot = self.slot(primary);
        let surface = slot.swap.as_ref().expect("a swap chain");
        match surface.get_current_texture() {
            Ok(frame) => {
                slot.frame_view = Some(frame.texture.create_view(&TextureViewDescriptor::default()));
                slot.frame = Some(frame);
                true
            }
            Err(_) => false,
        }
    }

    fn swap_begin_pass(&mut self, primary: SurfaceId) {
        let view = self.slot(primary).frame_view.clone().expect("an acquired frame");
        let encoder = self.encoder();
        let pass = encoder
            .begin_render_pass(&RenderPassDescriptor {
                label: None,
                color_attachments: &[Some(RenderPassColorAttachment {
                    view: &view,
                    resolve_target: None,
                    ops: Operations { load: LoadOp::Clear(Color::TRANSPARENT), store: StoreOp::Store },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
            })
            .forget_lifetime();
        self.pass = Some(pass);
        self.pass_pipeline = None;
    }

    fn swap_present(&mut self, primary: SurfaceId) {
        let slot = self.slot(primary);
        slot.frame_view = None;
        if let Some(frame) = slot.frame.take() {
            frame.present();
        }
    }
}

/// The wgpu surface target of the window's native kind (Win32 HWND, X11 Window, Wayland surface, Metal layer).
fn surface_target(native: &NativeWindow) -> Option<SurfaceTargetUnsafe> {
    use raw_window_handle::*;
    let handles = |display: RawDisplayHandle, window: RawWindowHandle| {
        Some(SurfaceTargetUnsafe::RawHandle { raw_display_handle: display, raw_window_handle: window })
    };
    match native.kind {
        PLAT_NATIVE_WIN32 => {
            let mut window = Win32WindowHandle::new(std::num::NonZeroIsize::new(native.window as isize)?);
            window.hinstance = std::num::NonZeroIsize::new(native.display as isize);
            handles(RawDisplayHandle::Windows(WindowsDisplayHandle::new()), RawWindowHandle::Win32(window))
        }
        PLAT_NATIVE_X11 => {
            let window = XlibWindowHandle::new(native.window as usize as std::ffi::c_ulong);
            let display = XlibDisplayHandle::new(NonNull::new(native.display as *mut c_void), 0);
            handles(RawDisplayHandle::Xlib(display), RawWindowHandle::Xlib(window))
        }
        PLAT_NATIVE_WAYLAND => {
            let window = WaylandWindowHandle::new(NonNull::new(native.window)?);
            let display = WaylandDisplayHandle::new(NonNull::new(native.display)?);
            handles(RawDisplayHandle::Wayland(display), RawWindowHandle::Wayland(window))
        }
        #[cfg(target_vendor = "apple")]
        PLAT_NATIVE_METAL => Some(SurfaceTargetUnsafe::CoreAnimationLayer(native.window)),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::device;
    use crate::directdraw::{self, DirectDraw};
    use crate::fake::{FakePlatform, PlatformBase};
    use crate::gx::Gx;
    use crate::keys::BlitMode;
    use crate::modes::DisplayInfo;

    fn try_start() -> Option<Gx<WgpuBackend, PlatformBase>> {
        let mut gx = Gx::new(WgpuBackend::default(), PlatformBase::default());
        // the plain path: the first working adapter (WARP on a machine without a GPU)
        if gx.start() {
            Some(gx)
        } else {
            eprintln!("skipped: no GPU adapter reachable (no software adapter either)");
            None
        }
    }

    #[test]
    fn limits_prefer_the_defaults_and_clamp_to_the_adapter() {
        let adapter = Limits { max_texture_dimension_2d: 4096, ..Limits::default() };
        let limits = base_limits(&adapter);
        assert_eq!(limits.max_texture_dimension_2d, 4096);
        assert_eq!(limits.max_bind_groups, Limits::default().max_bind_groups);
        let adapter = Limits::downlevel_webgl2_defaults();
        let limits = base_limits(&adapter);
        assert_eq!(limits.max_storage_buffers_per_shader_stage, 0);
    }

    #[test]
    fn numberings_are_webgpu_h_values() {
        assert_eq!(backend_number(Backend::Dx12), 4);
        assert_eq!(backend_number(Backend::Vulkan), 6);
        assert_eq!(device_type_number(DeviceType::Cpu), 3);
        assert_eq!(device_type_number(DeviceType::DiscreteGpu), 1);
    }

    #[test]
    fn native_kinds_without_a_surface_are_refused() {
        assert!(surface_target(&NativeWindow::default()).is_none());
        let n = NativeWindow { kind: PLAT_NATIVE_WIN32, ..Default::default() };
        assert!(surface_target(&n).is_none(), "a null HWND");
    }

    fn lock_read(gx: &mut Gx<WgpuBackend, PlatformBase>, id: SurfaceId) -> Vec<u32> {
        gx.for_cpu_read(id);
        let s = gx.s(id);
        let mut out = Vec::new();
        for y in 0..s.height {
            for x in 0..s.width {
                out.push(s.read_pixel(x, y));
            }
        }
        out
    }

    fn target(gx: &mut Gx<WgpuBackend, PlatformBase>, dd: &DirectDraw, w: u32, h: u32) -> SurfaceId {
        let desc = GxSurfaceDesc {
            dwFlags: (GXSD_WIDTH | GXSD_HEIGHT) as _,
            dwWidth: w as _,
            dwHeight: h as _,
            ddsCaps: GxSCaps { dwCaps: GXSCAPS_3DDEVICE as _, dwCaps2: 0 },
            ..Default::default()
        };
        directdraw::create_surface(gx, dd, &desc).unwrap()
    }

    /// A fill through the GPU and a read-back: real device, real shader, real map.
    #[test]
    fn device_gpu_fill_reads_back_exact() {
        let Some(mut gx) = try_start() else { return };
        let dd = DirectDraw::new(&DisplayInfo::default());
        let id = target(&mut gx, &dd, 8, 8);
        let (color, rect) = (Some([1.0f32, 0.0, 0.0, 1.0]), Rect::new(0, 0, 8, 8));
        gx.blit(id, &rect, None, None, BlitMode::Fill, color, 0.0, false);
        gx.submit();
        let pixels = lock_read(&mut gx, id);
        assert_eq!(pixels[0] & 0x00ff_ffff, 0x00ff_0000);
        assert!(pixels.iter().all(|p| *p == pixels[0]));
    }

    /// The fixed-function pipeline on the device: an unlit triangle covering the target, fill colour from the vertices.
    #[test]
    fn device_draw_runs_the_fixed_function_shader() {
        let Some(mut gx) = try_start() else { return };
        let dd = DirectDraw::new(&DisplayInfo::default());
        let id = target(&mut gx, &dd, 16, 16);
        let mut dev = directdraw::create_device(&mut gx, id);
        dev.set_render_state(GXRS_LIGHTING as u32, 0);
        dev.set_render_state(GXRS_CULLMODE as u32, 1);
        dev.set_stage_state(0, GXTSS_COLOROP as u32, u64::from(GXTOP_DISABLE));
        let mut vb = device::VertexBuffer::new(GXFVF_XYZ | GXFVF_NORMAL | GXFVF_DIFFUSE | GXFVF_TEX1, 3);
        let positions = [(-1.0f32, -1.0f32), (3.0, -1.0), (-1.0, 3.0)];
        for (i, (x, y)) in positions.iter().enumerate() {
            let v = &mut vb.vertices[i * 36..][..36];
            v[0..4].copy_from_slice(&x.to_le_bytes());
            v[4..8].copy_from_slice(&y.to_le_bytes());
            v[8..12].copy_from_slice(&0.5f32.to_le_bytes());
            v[24..28].copy_from_slice(&0xff00_ff00u32.to_le_bytes());
        }
        assert_eq!(device::draw(&mut gx, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None), GX_OK);
        gx.submit();
        let pixels = lock_read(&mut gx, id);
        assert!(pixels.iter().all(|p| *p & 0x00ff_ffff == 0x0000_ff00), "{:x}", pixels[0]);
        device::destroy_device(&mut gx, &mut dev);
    }

    /// Blit with a colour key on the device: the keyed texel keeps the destination's pixels.
    #[test]
    fn device_keyed_blit_discards_the_key() {
        let Some(mut gx) = try_start() else { return };
        let dd = DirectDraw::new(&DisplayInfo::default());
        let source_desc = GxSurfaceDesc {
            dwFlags: (GXSD_WIDTH | GXSD_HEIGHT | GXSD_CKSRCBLT) as _,
            dwWidth: 2,
            dwHeight: 1,
            ddsCaps: GxSCaps { dwCaps: GXSCAPS_TEXTURE as _, dwCaps2: 0 },
            ddckCKSrcBlt: GxColorKey { dwColorSpaceLowValue: 0x00ff00ff, dwColorSpaceHighValue: 0 },
            ..Default::default()
        };
        let source = directdraw::create_surface(&mut gx, &dd, &source_desc).unwrap();
        gx.for_cpu_write(source);
        gx.m(source).write_pixel(0, 0, 0x00ff00ff);
        gx.m(source).write_pixel(1, 0, 0x0000ff00);
        let dst = target(&mut gx, &dd, 2, 1);
        let rect = Rect::new(0, 0, 2, 1);
        gx.blit(dst, &rect, None, None, BlitMode::Fill, Some([0.0, 0.0, 1.0, 1.0]), 0.0, false);
        gx.blit(dst, &rect, Some(source), Some(&rect), BlitMode::Keyed, None, 0.0, false);
        gx.submit();
        let pixels = lock_read(&mut gx, dst);
        assert_eq!(pixels[0] & 0x00ff_ffff, 0x0000_00ff, "keyed: blue kept");
        assert_eq!(pixels[1] & 0x00ff_ffff, 0x0000_ff00);
    }

    #[test]
    fn device_scaled_target_round_trips_through_the_cpu() {
        let Some(mut gx) = try_start() else { return };
        gx.plat.pixels = Some((16, 16));
        let dd = DirectDraw { mode_width: 8, mode_height: 8, ..DirectDraw::new(&DisplayInfo::default()) };
        let id = target(&mut gx, &dd, 8, 8);
        assert_eq!(gx.s(id).scale, 2.0);
        assert_eq!(gx.s(id).tex_width(), 16);
        gx.blit(id, &Rect::new(0, 0, 8, 8), None, None, BlitMode::Fill, Some([0.0, 1.0, 0.0, 1.0]), 0.0, false);
        gx.submit();
        let pixels = lock_read(&mut gx, id);
        assert!(pixels.iter().all(|p| *p & 0x00ff_ffff == 0x0000_ff00), "{:x}", pixels[0]);
    }

    #[test]
    fn fake_platform_is_exercised_elsewhere() {
        // documents why `FakePlatform` is imported: the device tests use `PlatformBase`, the logic tests the fake
        let _ = FakePlatform::default();
    }
}
