"""Build offline collection guides from a pinned MIT guide archive and reviewed additions.

Download the archive URL below, then run:
    python tools/build_check_guides.py --archive /path/to/upstream.zip
Only Markdown is read; no upstream code is executed or extracted.
"""
import argparse
import hashlib
import json
import re
import zipfile
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
REVISION = 'f0b121a35b8b296f62fd35e7ff0517a65d42ce88'
ARCHIVE_URL = f'https://codeload.github.com/gleedgleed/gleeds-tracker/zip/{REVISION}'
ARCHIVE_SHA256 = 'd2f3b4a8a69e765021e4fbd9e6959b303afe4cfeda9aff3fc32010bb2ccbd794'
UPSTREAM = f'https://github.com/gleedgleed/gleeds-tracker/tree/{REVISION}/explanations'
WIKI = 'https://wiki.tprandomizer.com/index.php?title='
DUNGEON = 'https://www.zeldadungeon.net/twilight-princess-walkthrough/'


def plain(text):
    text = re.sub(r'\[([^]]+)\]\([^)]+\)', r'\1', text)
    text = text.replace('**', '').replace('`', '').replace('__', '')
    text = text.replace(' -- ', ' - ')
    return ' '.join(text.split()).strip()


def build(archive):
    payload = archive.read_bytes()
    if hashlib.sha256(payload).hexdigest() != ARCHIVE_SHA256:
        raise ValueError('Guide archive does not match the reviewed revision/checksum')
    prefix = f'gleeds-tracker-{REVISION}/'
    with zipfile.ZipFile(archive) as z:
        docs = {Path(n).stem: z.read(n).decode('utf-8') for n in z.namelist()
                if n.startswith(prefix+'explanations/') and n.endswith('.md')}
        license_text = z.read(prefix+'license.md').decode('utf-8')
    catalogue = json.loads((ROOT/'res/catalogue.json').read_text(encoding='utf-8'))
    overrides = json.loads((ROOT/'tools/check_guide_overrides.json').read_text(encoding='utf-8'))
    sources = {
        'gleed': {'label': "Gleed's Tracker", 'url': UPSTREAM},
        'wiki_hints': {'label': 'TPR Wiki: Hint locations', 'url': WIKI+'Hints'},
        'lake_cave': {'label': 'TPR Wiki: Long Lantern Cave', 'url': WIKI+'Long_Lantern_Cave'},
        'forest_walkthrough': {'label': 'Zelda Dungeon: Forest Temple', 'url': DUNGEON+'forest-temple/'},
        'ordon_walkthrough': {'label': 'Zelda Dungeon: Ordon Village', 'url': DUNGEON+'ordon-village/'},
        'sky_walkthrough': {'label': 'Zelda Dungeon: In Search of the Sky', 'url': DUNGEON+'in-search-of-the-sky/'},
    }
    for region, slug in [('faron','faron-woods'),('eldin','kakariko-village'),('lanayru','lanayru-province')]:
        sources['bugs_'+region] = {'label': 'Zelda Dungeon: '+region.capitalize()+' twilight', 'url': DUNGEON+slug+'-twilight-wii/'}
    guides = {}
    for check in catalogue['checks']:
        name = check['name']
        upstream_name = name.replace('City in the Sky','City in The Sky')
        if name == 'Defeat Ganondorf': upstream_name = 'Hyrule Castle Ganondorf'
        guide = {'steps': [], 'sources': []}
        if upstream_name in docs:
            markdown = docs[upstream_name]
            body = re.split(r'\n---\s*\n', markdown, maxsplit=1)[0]
            guide['steps'] = [plain(p) for p in re.split(r'\n\s*\n', body)
                              if p.strip() and not p.startswith(('#','Requires:', '*Source:'))]
            guide['sources'] = ['gleed']
            guide['upstream_name'] = upstream_name
            for label, url in re.findall(r'\[([^]]+)\]\((https://[^)]+)\)', markdown):
                sid = 'ref_'+hashlib.sha256(url.encode()).hexdigest()[:12]
                sources[sid] = {'label': plain(label), 'url': url}
                if sid not in guide['sources']: guide['sources'].append(sid)
        if name in overrides:
            guide.update(overrides[name])
        if not guide['steps'] or not guide['sources']:
            raise ValueError('Missing researched guide: '+name)
        guides[name] = guide
    unknown = set(overrides)-set(guides)
    if unknown: raise ValueError('Override for unknown check: '+str(sorted(unknown)))
    used = {sid for g in guides.values() for sid in g['sources']}
    sources = {sid: source for sid, source in sources.items() if sid in used}
    for source in sources.values():
        if urlsplit(source['url']).scheme != 'https': raise ValueError('Non-HTTPS source')
    result = {'version': 1, 'upstream_revision': REVISION, 'sources': sources, 'checks': guides}
    (ROOT/'res/check_guides.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    credit = f'''TPTracker collection-guide sources

Guides are bundled for offline use. No runtime web requests are made.

Most collection directions are adapted from Gleed's Tracker's MIT-licensed
explanations, pinned to {REVISION}.
{UPSTREAM}
Archive: {ARCHIVE_URL}
SHA256: {ARCHIVE_SHA256}

Gleed's Tracker acknowledges the Twilight Princess Randomizer Wiki, Zelda
Dungeon, Zelda Universe, Zelda Fandom and StrategyWiki as guide references.
Per-check source references are retained in check_guides.json. New concise
hint-sign and Twilit-insect directions use the cited walkthroughs and TPR Wiki.
No walkthrough images are redistributed.

Directions describe normal collection methods, not a definitive logic model.
TPTracker evaluates the seed's settings and inventory separately. Guide text
never imports the seed's randomized rewards. Directions use the unmirrored
GameCube layout; numbered checks at the same site share collection guidance.

Rebuild using tools/build_check_guides.py with the checksum-verified archive.
Reviewed corrections and missing-check additions are maintained in
 tools/check_guide_overrides.json.

Upstream license and third-party notice follow:

{license_text}
'''
    (ROOT/'res/CHECK_GUIDE_SOURCES.txt').write_text(credit,encoding='utf-8')
    print(f'Generated {len(guides)} offline check guides with {len(sources)} source references.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',required=True,type=Path)
    build(parser.parse_args().archive)
