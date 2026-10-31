#!/usr/bin/env python3
"""Check owned include boundaries and CMake component dependencies."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Reusable components must not depend on the sensor/actuator application.
COMPONENTS = {
    'platform': ROOT / 'components' / 'platform',
    'configuration': ROOT / 'components' / 'configuration',
    'networking': ROOT / 'components' / 'networking',
    'drivers': ROOT / 'components' / 'drivers',
    'domain': ROOT / 'applications' / 'node' / 'components' / 'domain',
    'application': ROOT / 'applications' / 'node' / 'components' / 'application',
    'node_drivers': ROOT / 'applications' / 'node' / 'components' / 'node_drivers',
    'wiring': ROOT / 'applications' / 'node' / 'components' / 'wiring',
}
ALLOWED = {
    'platform': set(),
    'configuration': set(),
    # The command envelope calls CommandIngress, so networking depends on the
    # node application. Platform, storage, and the SHT3x driver do not.
    'networking': {'configuration', 'domain', 'application'},
    'drivers': set(),
    'domain': set(),
    'application': {'domain'},
    'node_drivers': {'domain', 'application'},
    'wiring': {'domain', 'application', 'platform', 'drivers', 'node_drivers'},
}
PORTABLE = {'drivers', 'domain', 'application', 'node_drivers'}


def main() -> int:
    errors = []
    names = set(ALLOWED)
    for layer, allowed in ALLOWED.items():
        component = COMPONENTS[layer]
        for path in component.rglob('*'):
            if not path.is_file() or 'test' in path.parts or path.suffix not in {'.hpp', '.cpp'}:
                continue
            source = path.read_text()
            for include in re.findall(r'^\s*#include\s+"([^"]+)"', source, re.M):
                if include.split('/')[0] in names:
                    dependency = include.split('/')[0]
                    if dependency not in allowed | {layer}:
                        errors.append(f'{path.relative_to(ROOT)}: forbidden include {include}')
                elif layer in PORTABLE:
                    errors.append(f'{path.relative_to(ROOT)}: nonportable include {include}')
            if path.suffix == '.hpp' and re.search(r'\busing\s+namespace\b', source):
                errors.append(f'{path.relative_to(ROOT)}: public using-namespace directive')
        cmake = (component / 'CMakeLists.txt').read_text()
        for match in re.finditer(r'(?:PRIV_)?REQUIRES\s+([^)]*)', cmake):
            for dependency in match[1].split():
                if dependency in names and dependency not in allowed | {layer}:
                    errors.append(f'{layer}: forbidden component dependency {dependency}')
                elif layer in PORTABLE and dependency not in names:
                    errors.append(f'{layer}: nonportable component dependency {dependency}')
    for error in errors:
        print(error)
    if not errors:
        print('Architecture boundaries passed.')
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
