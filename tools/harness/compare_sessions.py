"""Compare the latest successful original/experimental game-session tours."""
import hashlib,json,pathlib,sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import exe_boot as boot


def main():
    root=boot.ROOT/'build/menu_validation'
    reports={};evidence={}
    for role,suffix in [('original','o'),('experimental','r')]:
        latest=json.loads((root/role/'latest.json').read_text(encoding='utf-8'))
        directory=pathlib.Path(latest['evidence']);evidence[role]=directory
        reports[role]=json.loads((directory/f'windowed_{suffix}.session.json').read_text(encoding='utf-8'))
    left=reports['original'];right=reports['experimental']
    labels=lambda report:[(row['milestone'],row['form']) for row in report['exercise']['milestones']]
    registries=[boot.windowed_config((evidence[role]/f'windowed_{suffix}.session.config.yaml').read_bytes(),boot.BOOT/f'windowed_{suffix}')
                for role,suffix in [('original','o'),('experimental','r')]]
    overlay_sources={}
    for extension in ('silent','presentation'):
        records=[]
        for role in ('original','experimental'):
            files=list(evidence[role].glob(f'*.{extension}.json'))
            if len(files)!=1:raise ValueError(f'Missing {role} {extension} manifest')
            records.append(json.loads(files[0].read_text(encoding='utf-8'))['source_sha256'])
        overlay_sources[extension]=records[0]==records[1]
    renderers=[json.loads(next(evidence[role].glob('*.software.json')).read_text(encoding='utf-8'))['output_sha256']
               for role in ('original','experimental')]
    overlay_sources['software_renderer']=renderers[0]==renderers[1]
    images={}
    for label,_ in labels(left):
        files=[evidence[role]/(label+'.bmp') for role in ('original','experimental')]
        images[label]=dict(original_sha256=hashlib.sha256(files[0].read_bytes()).hexdigest(),
                           experimental_sha256=hashlib.sha256(files[1].read_bytes()).hexdigest())
        images[label]['identical']=images[label]['original_sha256']==images[label]['experimental_sha256']
    telemetry={role:json.loads((directory/'simulation.json').read_text(encoding='utf-8')) for role,directory in evidence.items()}
    passed=(left['completed'] and right['completed'] and labels(left)==labels(right)
            and any(label=='gameplay' for label,_ in labels(right)) and registries[0]==registries[1]
            and all(overlay_sources.values()) and all(row['physics_advanced'] for row in telemetry.values())
            and all(not row['samples'][-1]['paused'] and not row['samples'][-1]['multiplayer'] for row in telemetry.values()))
    result=dict(test_only=True,passed=passed,evidence={key:str(value) for key,value in evidence.items()},
        matching_ui_milestones=labels(left)==labels(right),game_preferences_identical=registries[0]==registries[1],
        compatibility_sources_identical=overlay_sources,images=images,telemetry=telemetry,
        experimental_execution=right['experimental_execution'],
        limitations='Game seeds differ; image equality is diagnostic. Queued application events, not physical input. No combat/save-load/multiplayer or general semantic-equivalence claim.')
    (root/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(f'{"PASS" if passed else "FAIL"} paired menu/settings/gameplay tours; preferences equal={registries[0]==registries[1]}')
    print('Identical framebuffer captures: '+', '.join(label for label,row in images.items() if row['identical']))
    sys.exit(0 if passed else 1)


if __name__=='__main__':main()
