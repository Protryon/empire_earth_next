// C:\EEX\Low-Level Engine\Sound Engine\SESoundManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// SESoundManager, its instance nodes (SESMInstanceNode) and the samples (SESample) whole in Rust
// (rust/crates/ee_sound managerobj.rs, objects.rs; LLE lane round 6): every member is a forwarder. This file keeps
// the C++ the Rust reaches (extern "C" ee_soundcpp_*): ESVerify, the floating-point control, the sound path and its
// file specs.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/SESoundManager.h"
#include "rust/ee_sound.h"

// a none (-1) or paused (-2) id or channel keeps all bits set on LP64, as the C++ stored it (`= -1`)
static unsigned long Wide(uint32_t inValue)
{
    return inValue >= 0xfffffffeu ? (unsigned long)(long)(int32_t)inValue : (unsigned long)inValue;
}
#define RUST reinterpret_cast<EeSoundManager*>(this)
#define CRUST reinterpret_cast<const EeSoundManager*>(this)
#define NODE(n) reinterpret_cast<EeSoundNode*>(n)
#define SAMPLE reinterpret_cast<EeSoundSample*>(const_cast<SESample*>(this))

// ---- SESMInstanceNode (0x4f0d3..0x4f15c)
void SESMInstanceNode::Reference() { ee_sound_node_reference(NODE(this)); }
void SESMInstanceNode::Dereference() { ee_sound_node_dereference(NODE(this)); }

// ---- SESample (the original's inline members, 0x4c29a..0x4c47e)
SESample* SESample::Duplicate(const SESample& inSample) { return reinterpret_cast<SESample*>(ee_sound_sample_duplicate(reinterpret_cast<const EeSoundSample*>(&inSample))); }
void* SESample::GetData() const { return ee_sound_sample_data(SAMPLE); }
const char* SESample::GetFilename() const { return ee_sound_sample_filename(SAMPLE); }
bool SESample::GetIsLooping() const { return ee_sound_sample_is_looping(SAMPLE); }
unsigned long SESample::GetLengthInBytes() const { return ee_sound_sample_length_in_bytes(SAMPLE); }
unsigned long SESample::GetLengthInSamples() const { return ee_sound_sample_length_in_samples(SAMPLE); }
unsigned long SESample::GetMixerChannel() const { return Wide(ee_sound_sample_mixer_channel(SAMPLE)); }
float SESample::GetPan() const { return ee_sound_sample_pan(SAMPLE); }
float SESample::GetPitch() const { return ee_sound_sample_pitch(SAMPLE); }
unsigned long SESample::GetSampleBits() const { return ee_sound_sample_bits(SAMPLE); }
unsigned long SESample::GetSampleOffset() { return ee_sound_sample_offset(SAMPLE); }
unsigned long SESample::GetSampleRate() const { return ee_sound_sample_rate(SAMPLE); }
float SESample::GetVolume() const { return ee_sound_sample_volume(SAMPLE); }
long SESample::LoadWaveData(FSFileSpec* inSpec) { return ee_sound_sample_load_wave_data(SAMPLE, inSpec); }
void SESample::SetIsLooping(bool inLooping) { ee_sound_sample_set_is_looping(SAMPLE, inLooping); }
long SESample::SetOffset(unsigned long inOffset) { return ee_sound_sample_set_offset(SAMPLE, (uint32_t)inOffset); }
long SESample::SetPan(float inPan) { return ee_sound_sample_set_pan(SAMPLE, inPan); }
long SESample::SetPitch(float inPitch) { return ee_sound_sample_set_pitch(SAMPLE, inPitch); }
long SESample::SetVolume(float inVolume) { return ee_sound_sample_set_volume(SAMPLE, inVolume); }
void SESample::SetData(void* inData) { ee_sound_sample_set_data(SAMPLE, inData); }
void SESample::SetFilename(const char* inFilename) { ee_sound_sample_set_filename(SAMPLE, inFilename); }
void SESample::SetLengthInBytes(unsigned long inLength) { ee_sound_sample_set_length_in_bytes(SAMPLE, (uint32_t)inLength); }
void SESample::SetLengthInSamples(unsigned long inLength) { ee_sound_sample_set_length_in_samples(SAMPLE, (uint32_t)inLength); }
void SESample::SetMixerChannel(unsigned long inChannel) { ee_sound_sample_set_mixer_channel(SAMPLE, (uint32_t)inChannel); }
void SESample::SetSampleBits(unsigned long inBits) { ee_sound_sample_set_bits(SAMPLE, (uint32_t)inBits); }
void SESample::SetSampleRate(unsigned long inRate) { ee_sound_sample_set_rate(SAMPLE, (uint32_t)inRate); }
void SESample::SetTheIsLooping(bool inLooping) { ee_sound_sample_set_the_is_looping(SAMPLE, inLooping); }
void SESample::SetThePan(float inPan) { ee_sound_sample_set_the_pan(SAMPLE, inPan); }
void SESample::SetThePitch(float inPitch) { ee_sound_sample_set_the_pitch(SAMPLE, inPitch); }
void SESample::SetTheVolume(float inVolume) { ee_sound_sample_set_the_volume(SAMPLE, inVolume); }

// ---- SESoundManager
// FUNCTION: LLE 0x1004f15c
SESoundManager* SESoundManager::Create(FSPath* inPath, SEMixer* inMixer, unsigned long inCategoryCount,
                                       unsigned long inChannelCount)
{
    return reinterpret_cast<SESoundManager*>(ee_sound_manager_new(inPath, reinterpret_cast<EeSoundMixer*>(inMixer),
                                                                  (uint32_t)inCategoryCount, (uint32_t)inChannelCount));
}
// FUNCTION: LLE 0x1004f3bf
void SESoundManager::Destroy(SESoundManager* inManager) { ee_sound_manager_free(reinterpret_cast<EeSoundManager*>(inManager)); }
// FUNCTION: LLE 0x1004f8ee
void SESoundManager::CleanupMemoryPoolAndDestroyCriticalSections() { ee_sound_pool_cleanup(); }
// FUNCTION: LLE 0x100501eb
SESMInstanceNode* SESoundManager::GetAndReferenceSoundInstance(unsigned long inInstanceID) { return reinterpret_cast<SESMInstanceNode*>(ee_sound_manager_get_and_reference(RUST, (uint32_t)inInstanceID)); }
unsigned long SESoundManager::GetAvailableChannels() const { return ee_sound_manager_available_channels(CRUST); }
TSCriticalSection* SESoundManager::GetCriticalSection() const { return static_cast<TSCriticalSection*>(ee_sound_manager_lock(CRUST)); }
unsigned long SESoundManager::GetHighestTimerDelay() const { return ee_sound_manager_mixer_value(CRUST, 0); }
// FUNCTION: LLE 0x1004fc19
unsigned long SESoundManager::GetInstanceID(SESMInstanceNode* inNode) { return Wide(ee_sound_manager_node_instance_id(RUST, NODE(inNode))); }
// FUNCTION: LLE 0x1004fc46
bool SESoundManager::GetIsLooping(unsigned long inInstanceID) { return ee_sound_manager_is_looping(RUST, (uint32_t)inInstanceID); }
// FUNCTION: LLE 0x1004fca3
bool SESoundManager::GetIsLooping(SESMInstanceNode* inNode) { return ee_sound_manager_node_is_looping(RUST, NODE(inNode)); }
bool SESoundManager::GetIsMuted() { return ee_sound_manager_muted(RUST); }
// FUNCTION: LLE 0x1004fcda
bool SESoundManager::GetIsPlaying(unsigned long inInstanceID) { return ee_sound_manager_is_playing(RUST, (uint32_t)inInstanceID); }
// FUNCTION: LLE 0x1004fd38
bool SESoundManager::GetIsPlaying(SESMInstanceNode* inNode) { return ee_sound_manager_node_is_playing(RUST, NODE(inNode)); }
float SESoundManager::GetMasterVolume() const { return ee_sound_manager_master_volume(CRUST); }
unsigned long SESoundManager::GetMaxChannels() const { return ee_sound_manager_mixer_value(CRUST, 1); }
// FUNCTION: LLE 0x1004feb7
float SESoundManager::GetPan(unsigned long inInstanceID) { return ee_sound_manager_pan(RUST, (uint32_t)inInstanceID); }
// FUNCTION: LLE 0x1004ff16
float SESoundManager::GetPan(SESMInstanceNode* inNode) { return ee_sound_manager_node_pan(RUST, NODE(inNode)); }
FSPath* SESoundManager::GetPath() const { return static_cast<FSPath*>(ee_sound_manager_path(CRUST)); }
// FUNCTION: LLE 0x1004ff59
float SESoundManager::GetPitch(unsigned long inInstanceID) { return ee_sound_manager_pitch(RUST, (uint32_t)inInstanceID); }
// FUNCTION: LLE 0x1004ffb8
float SESoundManager::GetPitch(SESMInstanceNode* inNode) { return ee_sound_manager_node_pitch(RUST, NODE(inNode)); }
// FUNCTION: LLE 0x10050100
unsigned long SESoundManager::GetSampleID(unsigned long inInstanceID) { return Wide(ee_sound_manager_sample_id(RUST, (uint32_t)inInstanceID)); }
// FUNCTION: LLE 0x10050151
unsigned long SESoundManager::GetSampleID(SESMInstanceNode* inNode) { return Wide(ee_sound_manager_node_sample_id(RUST, NODE(inNode))); }
// FUNCTION: LLE 0x1005017e
unsigned long SESoundManager::GetSampleRate(unsigned long inInstanceID) { return ee_sound_manager_sample_rate(RUST, (uint32_t)inInstanceID); }
// FUNCTION: LLE 0x10050273
long SESoundManager::GetTimeLeftInMilli(unsigned long inInstanceID) { return ee_sound_manager_time_left(RUST, (uint32_t)inInstanceID); }
// FUNCTION: LLE 0x100502c6
long SESoundManager::GetTotalPlayTimeInMilli(unsigned long inSampleID) { return ee_sound_manager_total_play_time(RUST, (uint32_t)inSampleID); }
// FUNCTION: LLE 0x1005034f
float SESoundManager::GetVolume(unsigned long inInstanceID) { return ee_sound_manager_volume(RUST, (uint32_t)inInstanceID); }
// FUNCTION: LLE 0x100503f7
unsigned long SESoundManager::Load(const UString& inName, unsigned long inCategory, unsigned long inMaxInstances)
{
    return Wide(ee_sound_manager_load(RUST, (const char*)inName, (uint32_t)inCategory, (uint32_t)inMaxInstances));
}
long SESoundManager::MixAhead() { return ee_sound_manager_mix_ahead(RUST); }
// FUNCTION: LLE 0x100506df
unsigned long SESoundManager::Play(unsigned long inSampleID, float inVolume, float inPan, float inPitch, bool inLooping,
                                   char* inDebugName)
{
    return Wide(ee_sound_manager_play(RUST, (uint32_t)inSampleID, inVolume, inPan, inPitch, inLooping));
}
// FUNCTION: LLE 0x10050749
SESMInstanceNode* SESoundManager::PlayAndReturnReferencedNode(unsigned long inSampleID, float inVolume, float inPan,
                                                              float inPitch, bool inLooping, char* inDebugName)
{
    return reinterpret_cast<SESMInstanceNode*>(
        ee_sound_manager_play_node(RUST, (uint32_t)inSampleID, inVolume, inPan, inPitch, inLooping));
}
// FUNCTION: LLE 0x10050a9c
long SESoundManager::RegisterOneShotSound(unsigned long inSampleID, unsigned long inInstanceID) { return ee_sound_manager_register_one_shot(RUST, (uint32_t)inSampleID, (uint32_t)inInstanceID); }
// FUNCTION: LLE 0x1005154b
unsigned long SESoundManager::SetCategoryPercentage(unsigned long inCategory, float inPercent) { return ee_sound_manager_set_category_percentage(RUST, (uint32_t)inCategory, inPercent); }
// FUNCTION: LLE 0x10051985
long SESoundManager::SetCategoryPriority(unsigned long inCategory, unsigned long inPriority) { return ee_sound_manager_set_category_priority(RUST, (uint32_t)inCategory, (uint32_t)inPriority); }
void SESoundManager::SetIsMuted(bool inMuted) { ee_sound_manager_set_muted(RUST, inMuted); }
long SESoundManager::SetLatency(unsigned long inMilliseconds) { return ee_sound_manager_set_latency(RUST, (uint32_t)inMilliseconds); }
// FUNCTION: LLE 0x100515fb
long SESoundManager::SetMasterVolume(float inVolume) { return ee_sound_manager_set_master_volume(RUST, inVolume); }
// FUNCTION: LLE 0x10051610
long SESoundManager::SetMaxChannels(unsigned long inCount) { return ee_sound_manager_set_max_channels(RUST, (uint32_t)inCount); }
// FUNCTION: LLE 0x10051799
long SESoundManager::SetPan(unsigned long inInstanceID, float inPan) { return ee_sound_manager_set_pan(RUST, (uint32_t)inInstanceID, inPan); }
// FUNCTION: LLE 0x100517d1
long SESoundManager::SetPan(SESMInstanceNode* inNode, float inPan) { return ee_sound_manager_node_set(RUST, NODE(inNode), 1, inPan); }
// FUNCTION: LLE 0x10051778
void SESoundManager::SetPath(FSPath* inPath) { ee_sound_manager_set_path(RUST, inPath); }
// FUNCTION: LLE 0x1005188f
long SESoundManager::SetPitch(unsigned long inInstanceID, float inPitch) { return ee_sound_manager_set_pitch(RUST, (uint32_t)inInstanceID, inPitch); }
// FUNCTION: LLE 0x100518c7
long SESoundManager::SetPitch(SESMInstanceNode* inNode, float inPitch) { return ee_sound_manager_node_set(RUST, NODE(inNode), 2, inPitch); }
// FUNCTION: LLE 0x100519a2
long SESoundManager::SetSoundParameters(unsigned long inInstanceID, float inVolume, float inPan, float inPitch) { return ee_sound_manager_set_parameters(RUST, (uint32_t)inInstanceID, inVolume, inPan, inPitch); }
// FUNCTION: LLE 0x10051a5d
long SESoundManager::SetSoundParameters(SESMInstanceNode* inNode, float inVolume, float inPan, float inPitch) { return ee_sound_manager_node_set_parameters(RUST, NODE(inNode), inVolume, inPan, inPitch); }
// FUNCTION: LLE 0x10051af7
long SESoundManager::SetVolume(unsigned long inInstanceID, float inVolume) { return ee_sound_manager_set_volume(RUST, (uint32_t)inInstanceID, inVolume); }
// FUNCTION: LLE 0x10051b2f
long SESoundManager::SetVolume(SESMInstanceNode* inNode, float inVolume) { return ee_sound_manager_node_set(RUST, NODE(inNode), 0, inVolume); }
// FUNCTION: LLE 0x10051bf1
long SESoundManager::Stop(unsigned long inInstanceID, bool inLetFinish) { return ee_sound_manager_stop(RUST, (uint32_t)inInstanceID, inLetFinish); }
// FUNCTION: LLE 0x10051e31
long SESoundManager::Stop(SESMInstanceNode* inNode, bool inImmediately) { return ee_sound_manager_stop_node(RUST, NODE(inNode), inImmediately); }
// FUNCTION: LLE 0x10051e72
long SESoundManager::StopAllOneShotSounds() { return ee_sound_manager_stop_all_one_shots(RUST); }
// FUNCTION: LLE 0x10051ece
long SESoundManager::Unload(unsigned long inSampleID) { return ee_sound_manager_unload(RUST, (uint32_t)inSampleID); }

// ---- the C++ the Rust objects reach (rust/crates/ee_sound sys.rs)
extern "C" {
void ee_soundcpp_verify(const char* inExpression, uint32_t inLine) { ESVerify(__FILE__, inLine, inExpression); }
void ee_soundcpp_fp_precision53() { UMSetFloatingPointPrecision(kFPPrecision53); }
void ee_soundcpp_fp_check()
{
    if (UMGetFloatingPointPrecision() != kFPPrecision53)
        UMSetFloatingPointPrecision(kFPPrecision53);
    if (UMGetFloatingPointRoundingMode() != 0)
        UMSetFloatingPointRoundingMode(0);
}
void ee_soundcpp_path_reference(void* inPath) { static_cast<FSPath*>(inPath)->Reference(); }
void ee_soundcpp_path_dereference(void* inPath) { static_cast<FSPath*>(inPath)->Dereference(); }
// FindFile on the path (NULL: not found; a spec the path returned with an error leaks, as before)
void* ee_soundcpp_find_file(void* inPath, const char* inName)
{
    FSFileSpec* theFileSpec = NULL;
    if (static_cast<FSPath*>(inPath)->FindFile(UString(inName), theFileSpec) != kErrorOK)
        return NULL;
    return theFileSpec;
}
void ee_soundcpp_file_delete(void* inFile) { delete static_cast<FSFileSpec*>(inFile); }
}
