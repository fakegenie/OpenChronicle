import sys

from fontTools.ttLib import TTFont

WEIGHTS = [("medium", "fonts/PixelifySans-Medium.ttf"), ("bold", "fonts/PixelifySans-Bold.ttf")]
CODEPOINTS = list(range(32, 127)) + [0xB7, 0x2026]


def contours(font, name):
    glyf = font["glyf"]
    glyph = glyf[name]
    if glyph.numberOfContours == 0:
        return []
    coordinates, ends, flags = glyph.getCoordinates(glyf)
    shapes = []
    start = 0
    for end in ends:
        shapes.append([(int(coordinates[index][0]), int(coordinates[index][1]), flags[index] & 1) for index in range(start, end + 1)])
        start = end + 1
    return shapes


def pair_values(subtable, names):
    values = {}
    covered = subtable.Coverage.glyphs
    if subtable.Format == 1:
        for index, first in enumerate(covered):
            if first not in names:
                continue
            for record in subtable.PairSet[index].PairValueRecord:
                if record.SecondGlyph in names:
                    values[(first, record.SecondGlyph)] = getattr(record.Value1, "XAdvance", 0) or 0
        return values, set()
    firsts = subtable.ClassDef1.classDefs
    seconds = subtable.ClassDef2.classDefs
    for first in covered:
        if first not in names:
            continue
        row = subtable.Class1Record[firsts.get(first, 0)]
        for second in names:
            record = row.Class2Record[seconds.get(second, 0)]
            values[(first, second)] = getattr(record.Value1, "XAdvance", 0) or 0
    return values, set(covered)


def kerning(font, names):
    if "GPOS" not in font:
        return {}
    table = font["GPOS"].table
    lookups = set()
    for record in table.FeatureList.FeatureRecord:
        if record.FeatureTag == "kern":
            lookups.update(record.Feature.LookupListIndex)
    totals = {}
    for index in sorted(lookups):
        lookup = table.LookupList.Lookup[index]
        claimed = set()
        for subtable in lookup.SubTable:
            if lookup.LookupType == 9:
                subtable = subtable.ExtSubTable
            if subtable.LookupType != 2:
                continue
            values, whole = pair_values(subtable, names)
            for pair, amount in values.items():
                if pair in claimed:
                    continue
                claimed.add(pair)
                totals[pair] = totals.get(pair, 0) + amount
            claimed.update((first, second) for first in whole for second in names)
    return {pair: amount for pair, amount in totals.items() if amount}


def numbers(values, per_line):
    lines = []
    for start in range(0, len(values), per_line):
        lines.append("    " + ", ".join(str(value) for value in values[start:start + per_line]) + ",")
    return "\n".join(lines)


def emit_weight(label, path):
    font = TTFont(path)
    cmap = font.getBestCmap()
    hmtx = font["hmtx"]
    shapes = []
    glyphs = []
    names = []
    for codepoint in CODEPOINTS:
        name = cmap.get(codepoint, cmap[ord("?")])
        names.append(name)
        outline = contours(font, name)
        points = [point for contour in outline for point in contour]
        xs = [point[0] for point in points] or [0]
        ys = [point[1] for point in points] or [0]
        glyphs.append((codepoint, hmtx[name][0], min(xs), min(ys), max(xs), max(ys), len(shapes)))
        shapes.append(len(outline))
        for contour in outline:
            shapes.append(len(contour))
            for x, y, on in contour:
                shapes.extend((x, y, on))
    index_of = {}
    for index, name in enumerate(names):
        index_of.setdefault(name, index)
    pairs = kerning(font, set(names))
    kerns = sorted((index_of[first] << 8 | index_of[second], amount) for (first, second), amount in pairs.items())
    hhea = font["hhea"]
    out = []
    out.append("static const short ph_inter_%s_shapes[] = {\n%s\n};\n" % (label, numbers(shapes, 18)))
    rows = "\n".join("    { %d, %d, %d, %d, %d, %d, %d }," % glyph for glyph in glyphs)
    out.append("static const struct ph_font_glyph ph_inter_%s_glyphs[] = {\n%s\n};\n" % (label, rows))
    rows = "\n".join("    { %d, %d }," % kern for kern in kerns)
    out.append("static const struct ph_font_kern ph_inter_%s_kerns[] = {\n%s\n};\n" % (label, rows))
    face = "    { %d, %d, %d, %d, %d, ph_inter_%s_glyphs, ph_inter_%s_shapes, ph_inter_%s_kerns }," % (
        font["head"].unitsPerEm, hhea.ascent, hhea.descent, len(glyphs), len(kerns), label, label, label)
    return "\n".join(out), face


def main():
    parts = []
    faces = []
    for label, path in WEIGHTS:
        text, face = emit_weight(label, path)
        parts.append(text)
        faces.append(face)
    header = """#ifndef PH_INTER_H
#define PH_INTER_H

enum {
    PH_INTER_GLYPHS = %d,
};

struct ph_font_glyph {
    int codepoint;
    int advance;
    int left;
    int bottom;
    int right;
    int top;
    int shape;
};

struct ph_font_kern {
    int pair;
    int amount;
};

struct ph_font {
    int units;
    int ascent;
    int descent;
    int glyph_count;
    int kern_count;
    const struct ph_font_glyph *glyphs;
    const short *shapes;
    const struct ph_font_kern *kerns;
};

""" % len(CODEPOINTS)
    footer = "static const struct ph_font ph_inter_fonts[2] = {\n%s\n};\n\n#endif\n" % "\n".join(faces)
    sys.stdout.write(header + "\n".join(parts) + "\n" + footer)


main()
