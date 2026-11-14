"""Rewrite resources.media in package.json from the images convert_logos.py
made. Each team gets LOGO_<TEAM> and HALF_<TEAM>, defined once per screen
shape so every platform gets the size made for it. The menu icon comes first.

Usage: python tools/gen_resources.py
"""

import json
import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
IMAGES = os.path.join(ROOT, 'resources', 'images')

PLATFORMS = {
    'rect': ['basalt'],
    'bw': ['diorite', 'flint'],
    'chalk': ['chalk'],
    'emery': ['emery'],
    'gabbro': ['gabbro'],
}


def main():
    teams = sorted(f[5:-4] for f in os.listdir(os.path.join(IMAGES, 'rect'))
                   if f.startswith('logo_') and f.endswith('.png'))
    # The icon in the watch's watchface list: color, and a black and white copy.
    media = [
        {
            'type': 'bitmap',
            'name': 'MENU_ICON',
            'file': 'images/menu_icon.png',
            'menuIcon': True,
            'targetPlatforms': ['basalt', 'chalk', 'emery', 'gabbro'],
        },
        {
            'type': 'bitmap',
            'name': 'MENU_ICON',
            'file': 'images/bw/menu_icon.png',
            'menuIcon': True,
            'targetPlatforms': ['diorite', 'flint'],
        },
    ]
    for team in teams:
        for prefix in ('logo', 'half'):
            for shape, platforms in PLATFORMS.items():
                media.append({
                    'type': 'bitmap',
                    'name': '%s_%s' % (prefix.upper(), team.upper()),
                    'file': 'images/%s/%s_%s.png' % (shape, prefix, team),
                    'targetPlatforms': platforms,
                })
    path = os.path.join(ROOT, 'package.json')
    with open(path) as f:
        pkg = json.load(f)
    pkg['pebble']['resources']['media'] = media
    with open(path, 'w', newline='\n') as f:
        json.dump(pkg, f, indent=2)
        f.write('\n')
    print('%d teams, %d resources' % (len(teams), len(media)))


if __name__ == '__main__':
    main()
