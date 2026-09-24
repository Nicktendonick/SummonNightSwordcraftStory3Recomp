"""Read-only field-animation resource/descriptor audit, without image inspection.

Use a field-provenance capture inventory or explicit saved states. Reports are
private metadata, not ROM assets, and are never rendering authorization. Source:
08094A4C -> 08005D6C/08001DE8; clocks 08005560; archive extents 08001D88.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

from audit_field_provenance import ROM_HASHES, field_state, inspect_layers, span, u16, u32
from inspect_scene_state import memory

ROOT = Path(__file__).resolve().parents[1]
ROM_BASE = 0x08000000


def member_extent(rom, base, size, index):
    """Bound both words of one eight-byte member entry by its parent archive."""
    if base < 0 or base % 4 or not 0 <= index < 0xffff:
        raise ValueError('invalid archive selection')
    archive = span(rom, base, size)
    entry = 8 + index * 8
    relative, length = u32(archive, entry) * 16, u32(archive, entry + 4) * 16
    if relative < entry + 8 or not length:
        raise ValueError('empty archive member or member inside table')
    span(archive, relative, length)
    return base + relative, length


def clock_valid(frames, durations, next_frame, timer):
    """Conservative bound including initialization and individual store states.

    The low duration byte is effective. Zero rolls through 255 after decrement,
    so it is a 256-tick interval. A timer bound is not visible-phase evidence.
    """
    if not frames or len(durations) != frames or not 0 <= timer <= 255:
        return False
    if next_frame == frames:
        return timer == (durations[-1] & 255)
    if not 0 <= next_frame < frames:
        return False
    previous = (next_frame - 1) % frames
    limits = [durations[n] & 255 for n in (previous, next_frame)]
    return 0 in limits or timer <= max(limits)


def family(data, table, index):
    offset = u16(data, table + index * 2)
    start = table + offset
    header = span(data, start, 4)
    frames, unused, width, height = header
    if not frames or not width or not height:
        raise ValueError('zero frame count or family dimensions')
    if offset < (index + 1) * 2 or offset % 2:
        raise ValueError('family overlaps its table entry or is misaligned')
    stride = 2 * (1 + width * height)
    span(data, start + 4, frames * stride)
    durations = [u16(data, start + 4 + n * stride) for n in range(frames)]
    return dict(index=index, table_relative_offset=offset,
                resource_relative_offset=start, frames=frames, unused_header_byte=unused,
                width=width, height=height, stride_bytes=stride,
                durations=durations, effective_ticks=[(x & 255) or 256 for x in durations],
                end_offset=start + 4 + frames * stride)


def inspect_resource(rom, resource, size):
    """Decode only layout/clock metadata; tilemap entries are not exported."""
    data = span(rom, resource, size)
    span(data, 0, 32)
    count, table_field = u16(data, 0x14), u16(data, 0x16)
    list_raw, table_raw = u32(data, 0x18), u32(data, 0x1c)
    list_offset, table_offset = list_raw & ~3, table_raw & ~3
    if count > 64:
        raise ValueError('placement count exceeds physical counter arrays')
    if count and (list_offset < 32 or table_offset < 32):
        raise ValueError('placement or family data overlaps resource header')
    span(data, list_offset, count * 6)
    placements = []
    referenced = set()
    for n in range(count):
        p = span(data, list_offset + n * 6, 6)
        placements.append(dict(index=n, x=p[0], y=p[1], family=p[2], initial_next=p[3],
                               unused_bytes=list(p[4:6])))
        referenced.add(p[2])
    families = {index: family(data, table_offset, index) for index in sorted(referenced)}
    # +16 is copied by the loader, but not consulted by the regular animator.
    # Test the family-count hypothesis independently instead of assuming it.
    declared = []
    declared_error = None
    if table_field <= 256:
        try:
            for index in range(table_field):
                parsed = families.get(index) or family(data, table_offset, index)
                declared.append(parsed)
        except ValueError as error:
            declared_error = str(error)
    else:
        declared_error = 'header +16 exceeds byte-indexed table capacity'
    table_spans_before_families = None
    declared_families_do_not_overlap = None
    if declared and not declared_error:
        table_spans_before_families = min(x['table_relative_offset'] for x in declared) >= table_field * 2
        ordered = sorted({(x['resource_relative_offset'], x['end_offset']) for x in declared})
        declared_families_do_not_overlap = all(a[1] <= b[0] for a, b in zip(ordered, ordered[1:]))
    for placement in placements:
        f = families[placement['family']]
        placement.update(width=f['width'], height=f['height'],
                         initial_index_valid=placement['initial_next'] < f['frames'])
    return dict(resource=hex(ROM_BASE + resource), size=size, header_10=u16(data, 0x10),
                header_12=u16(data, 0x12), count=count, header_16=table_field,
                list_offset_raw=list_raw, table_offset_raw=table_raw,
                list_offset=list_offset, table_offset=table_offset,
                list_pointer=hex(ROM_BASE + resource + list_offset),
                table_pointer=hex(ROM_BASE + resource + table_offset),
                referenced_families=sorted(referenced), placements=placements,
                families=list(families.values()), declared_families=declared,
                header_16_count_hypothesis=dict(
                    all_references_below_field=all(n < table_field for n in referenced),
                    all_declared_families_parse=declared_error is None,
                    declared_parse_error=declared_error,
                    table_ends_before_family_data=table_spans_before_families,
                    family_blocks_do_not_overlap=declared_families_do_not_overlap,
                    last_family_end_matches_header_12=(max((x['end_offset'] for x in declared), default=0) == u16(data, 0x12)) if declared else None,
                    first_family_relative_offset=min((x['table_relative_offset'] for x in declared), default=None)))


def inspect_layer(e, i, rom, state, map_info, archive, archive_size):
    bg = map_info['bg']
    f = int(state['pointer'], 16) - 0x02000000
    record, d = f + 0x4e0 + (bg - 1) * 0x2c, 0x2a20 + bg * 0x34
    asset = u16(e, record + 0x1e)
    count, base, allocated = u16(i, d + 0x10), u16(i, d + 0x14), u16(i, d + 0x16)
    result = dict(bg=bg, map_asset=map_info.get('asset'), asset=asset, authenticated=False,
                  map_authenticated=map_info['authenticated'], descriptor=dict(
                      count=count, field_12=u16(i, d + 0x12), base=base, allocated=allocated,
                      list_pointer=hex(u32(i, d + 0x28)), table_pointer=hex(u32(i, d + 0x2c))))
    problems = []
    if not map_info['authenticated']:
        problems.append('base map not authenticated')
    if base + count > 64 or base + allocated > 64:
        problems.append('counter allocation exceeds physical 64-byte arrays')
    if count > allocated:
        problems.append('active placements exceed allocated count')
    if asset == 0xffff:
        if count:
            problems.append('active animation count with absent asset')
        result.update(absent=True, count=count, problems=problems, authenticated=not problems)
        return result
    resource, size = member_extent(rom, archive, archive_size, asset)
    decoded = inspect_resource(rom, resource, size)
    result['resource_data'] = decoded
    result['count'] = decoded['count']
    if count != decoded['count']:
        problems.append('loaded/resource placement count disagreement')
    if count:
        if result['descriptor']['field_12'] != decoded['header_16']:
            problems.append('loaded/resource +12/+16 disagreement')
        if result['descriptor']['list_pointer'] != decoded['list_pointer']:
            problems.append('loaded/resource list pointer disagreement')
        if result['descriptor']['table_pointer'] != decoded['table_pointer']:
            problems.append('loaded/resource family table pointer disagreement')
        if allocated != count:
            problems.append('loaded/resource allocation disagreement')
    else:
        # A zero placement count needs no dereferenced live animation pointers.
        result['zero_count_live_pointers_not_required'] = True
    by_family = {x['index']: x for x in decoded['families']}
    overlaps = []
    occupied = {}
    for placement in decoded['placements']:
        n, x, y = placement['index'], placement['x'], placement['y']
        w, h = placement['width'], placement['height']
        placement['inside_map'] = x + w <= map_info.get('width', 0) // 8 and y + h <= map_info.get('height', 0) // 8
        if not placement['inside_map']:
            problems.append('placement outside authenticated map')
        if not placement['initial_index_valid']:
            problems.append('initial frame index outside family')
        next_frame, timer = span(i, 0x2d50 + base + n, 1)[0], span(i, 0x2af0 + base + n, 1)[0]
        fam = by_family[placement['family']]
        valid = clock_valid(fam['frames'], fam['durations'], next_frame, timer)
        placement.update(next=next_frame, timer=timer, clock_valid=valid)
        if not valid:
            problems.append('clock outside initialization/steady/transient bounds')
        for row in range(y, y + h):
            for col in range(x, x + w):
                if (col, row) in occupied:
                    pair = (occupied[col, row], n)
                    if pair not in overlaps:
                        overlaps.append(pair)
                occupied[col, row] = n
    result.update(overlapping_placements=overlaps, problems=sorted(set(problems)),
                  authenticated=not problems,
                  regular_extension_candidate=not problems and not overlaps)
    return result


def inspect(e, i, rom):
    result = dict(render_authorized=False, ownership_valid=False)
    try:
        state = field_state(e, i)
        result.update(state=state, ownership_valid=True)
        if not state['owned']:
            return result
        root = u32(i, 0x2974) - ROM_BASE
        archive, archive_size = member_extent(rom, root, len(rom) - root, 2)
        result.update(archive=hex(ROM_BASE + archive), archive_size=archive_size,
                      global_counter_allocation=u32(i, 0x29b0), layers=[])
        for map_info in inspect_layers(e, i, rom, state):
            try:
                result['layers'].append(inspect_layer(e, i, rom, state, map_info, archive, archive_size))
            except (ValueError, struct.error) as error:
                result['layers'].append(dict(bg=map_info['bg'], authenticated=False, reason=str(error)))
        slots = []
        for bg in range(4):
            d = 0x2a20 + bg * 0x34
            count, base = u16(i, d + 0x16), u16(i, d + 0x14)
            if count:
                slots.append(dict(bg=bg, base=base, count=count))
        ranges = [set(range(x['base'], x['base'] + x['count'])) for x in slots]
        result['counter_allocation'] = dict(
            ranges=slots, sum=sum(x['count'] for x in slots),
            ranges_within_physical_arrays=all(x['base'] + x['count'] <= 64 for x in slots),
            overlapping_bgs=[[slots[a]['bg'], slots[b]['bg']] for a in range(len(slots))
                             for b in range(a + 1, len(slots)) if ranges[a] & ranges[b]],
            sum_matches_global=sum(x['count'] for x in slots) == u32(i, 0x29b0))
        result['scripted_animation_active'] = any(span(i, 0x29c0 + n * 8, 1)[0] for n in range(12))
        result['all_animation_sources_authenticated'] = all(x['authenticated'] for x in result['layers'])
    except (ValueError, struct.error) as error:
        result['reason'] = str(error)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--inventory', type=Path, help='audit_field_provenance JSON inventory')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('states', nargs='*', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'validation'):
        parser.error('Reports must remain in this worktree validation directory')
    if output.exists():
        parser.error('Refusing to overwrite an existing report')
    rom = args.rom.read_bytes()
    digest = hashlib.sha256(rom).hexdigest()
    if digest not in ROM_HASHES:
        parser.error('Unreviewed ROM revision')
    states = set()
    expected = {}
    if args.inventory:
        inventory = json.loads(args.inventory.read_text(encoding='utf-8'))
        if inventory.get('rom_sha256') != digest:
            parser.error('Inventory/ROM identity disagreement')
        for record in inventory['records']:
            path = Path(record['state_file']).resolve()
            states.add(path)
            expected[path] = record['sha256']
    for path in args.states:
        states.update(f.resolve() for f in (path.rglob('*.gbas') if path.is_dir() else [path]))
    if not states:
        parser.error('No saved states found')
    records = []
    for path in sorted(states):
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        if path in expected and expected[path] != before:
            parser.error('Capture changed since inventory: ' + str(path))
        try:
            e, i = memory(path)
            row = inspect(e, i, rom)
        except (ValueError, struct.error) as error:
            row = dict(ownership_valid=False, render_authorized=False, reason=str(error))
        if hashlib.sha256(path.read_bytes()).hexdigest() != before:
            parser.error('Capture changed during read-only audit: ' + str(path))
        row.update(state_file=str(path), sha256=before)
        records.append(row)
    counts = Counter(captures=len(records))
    resources = {}
    for row in records:
        counts['field_owned'] += int(row.get('state', {}).get('owned', False))
        counts['all_animation_sources_authenticated'] += int(row.get('all_animation_sources_authenticated', False))
        for layer in row.get('layers', []):
            counts['layers'] += 1
            counts['animation_layers_authenticated'] += int(layer['authenticated'])
            counts['layers_with_placements'] += int(layer.get('count', 0) > 0)
            counts['layers_with_overlapping_placements'] += int(bool(layer.get('overlapping_placements')))
            data = layer.get('resource_data')
            if data:
                # No live counter values in the unique authored-resource summary.
                copy = {k: v for k, v in data.items() if k != 'placements'}
                resources[layer['asset']] = copy
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(dict(rom_sha256=digest, edition=ROM_HASHES[digest],
        summary=dict(counts), unique_resources=resources, records=records), indent=2), encoding='utf-8')
    print(json.dumps(dict(counts)))


if __name__ == '__main__':
    main()
