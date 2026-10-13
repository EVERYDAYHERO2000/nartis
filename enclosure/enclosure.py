"""Wall-mount enclosure for the Nartis RF reader (ESP32-C3 Super Mini + E07-M1101D).

Build:  pip install cadquery && python enclosure.py
  nartis_print_plate.3mf / .stl / .step  both parts laid out on the print bed
  nartis_box.stl, nartis_lid.stl (+ .step) each part separately, in print orientation

Layout (the box is screwed to the wall by its floor):
  +X end : SMA of the E07-M1101D exits through a U-slot; a tongue on the lid
           closes the slot above the connector. Mount with this end UP.
  -X end : USB-C of the ESP32-C3 Super Mini.
  sides  : two countersunk screw ears.
  lid    : snap fit - a ridge on the lid lip clicks into a groove in the walls.

All sizes in mm. Measure your boards and adjust the parameters below.
"""
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape

import cadquery as cq

# ---------------------------------------------------------------- parameters
WALL = 2.0            # side wall thickness
FLOOR = 2.0           # floor thickness
CLR = 0.4             # clearance around boards
CORNER_R = 2.0        # outer vertical edge radius

# boards
E07_L, E07_W, E07_PCB = 30.0, 15.0, 1.6      # E07-M1101D-SMA board (without SMA)
ESP_L, ESP_W, ESP_PCB = 22.52, 18.0, 1.0     # ESP32-C3 Super Mini board
ESP_USB_OVERHANG = 1.5                       # USB-C shell sticks out past the PCB edge

SUPPORT_H = 3.5       # boards rest this high above the floor (clears solder tails)
SMA_R = 3.4           # SMA thread 1/4" (6.35 mm) + clearance: radius of the wall slot
SMA_EDGE_GAP = 1.0    # E07 board edge to the inner +X wall
USB_FACE_GAP = 0.5    # USB-C face to the inner -X wall
USB_OPEN_W, USB_OPEN_H, USB_OPEN_R = 13.0, 7.5, 2.0   # opening for the USB-C plug overmold

# cavity
LANE_GAP = 4.0        # gap between the two boards (wires)
SIDE_MARGIN = 1.5     # board lane to side wall
INNER_L = 36.0        # inner length along X
INNER_H = 17.0        # inner height (header + bent soldered wires)

# lid
LID_T = 2.0
LIP_H = 3.0           # lip depth into the box
LIP_T = 1.6           # lip wall thickness
LIP_CLR = 0.25        # lip to wall clearance
SNAP_Z = 1.5          # snap ridge / groove centre below the rim
SNAP_R = 0.6          # ridge radius on the lip
GROOVE_R = 0.6        # groove radius in the wall
SNAP_END = 4.0        # ridge / groove stop this far from the corners

# ears
EAR_LEN, EAR_OUT, EAR_T = 12.0, 10.0, 3.0    # along the wall, out from the wall, thickness
SCREW_D, SCREW_HEAD_D = 4.2, 8.0             # 3.5-4 mm wood screw, 90-degree countersink

# guides holding each board
GUIDE_T, GUIDE_LEG, GUIDE_ABOVE = 1.2, 3.0, 1.5
RAIL_W = 1.6

# ---------------------------------------------------------------- derived
INNER_W = SIDE_MARGIN + E07_W + LANE_GAP + ESP_W + SIDE_MARGIN
OUT_L, OUT_W = INNER_L + 2 * WALL, INNER_W + 2 * WALL

E07_Y0 = SIDE_MARGIN
E07_X1 = INNER_L - SMA_EDGE_GAP              # SMA end of the E07 board
E07_X0 = E07_X1 - E07_L
SMA_Y = E07_Y0 + E07_W / 2
SMA_Z = SUPPORT_H + E07_PCB / 2              # edge-launch SMA straddles the PCB

ESP_Y0 = E07_Y0 + E07_W + LANE_GAP
ESP_X0 = USB_FACE_GAP + ESP_USB_OVERHANG
ESP_X1 = ESP_X0 + ESP_L
USB_Y = ESP_Y0 + ESP_W / 2
USB_Z = SUPPORT_H + ESP_PCB + 1.6            # USB-C receptacle is ~3.2 mm tall

# Coordinates: the inner cavity spans x 0..INNER_L, y 0..INNER_W, floor top at z = 0.


def block(x0, x1, y0, y1, z0, z1):
    """Axis-aligned box between two corners."""
    xa, xb = sorted((x0, x1))
    ya, yb = sorted((y0, y1))
    za, zb = sorted((z0, z1))
    return cq.Workplane("XY").box(xb - xa, yb - ya, zb - za, centered=False).translate((xa, ya, za))


def cyl_x(y, z, r, x0, x1):
    """Cylinder along X."""
    return cq.Workplane("YZ").workplane(offset=x0).center(y, z).circle(r).extrude(x1 - x0)


def board_guides(x0, y0, length, width, pcb):
    """Four L-shaped corner guides around a board outline."""
    h = SUPPORT_H + pcb + GUIDE_ABOVE
    out = None
    for cx, sx in ((x0, -1), (x0 + length, 1)):
        for cy, sy in ((y0, -1), (y0 + width, 1)):
            ox, oy = cx + sx * CLR, cy + sy * CLR          # outer edge of the clearance
            leg_a = block(ox, ox + sx * GUIDE_T, oy + sy * GUIDE_T, oy - sy * GUIDE_LEG, 0, h)
            leg_b = block(ox + sx * GUIDE_T, ox - sx * GUIDE_LEG, oy, oy + sy * GUIDE_T, 0, h)
            part = leg_a.union(leg_b)
            out = part if out is None else out.union(part)
    return out


def screw_ear(y_sign):
    """Ear on the +Y (y_sign=1) or -Y (y_sign=-1) side, flush with the floor."""
    wall_face = INNER_W + WALL if y_sign > 0 else -WALL
    x_c = INNER_L / 2
    ear = block(x_c - EAR_LEN / 2, x_c + EAR_LEN / 2,
                wall_face - y_sign * 1.0, wall_face + y_sign * EAR_OUT,
                -FLOOR, -FLOOR + EAR_T)
    ear = ear.edges("|Z").edges(">Y" if y_sign > 0 else "<Y").fillet(EAR_LEN / 2 - 0.5)
    hole_y = wall_face + y_sign * EAR_OUT / 2
    top = -FLOOR + EAR_T
    sink = (SCREW_HEAD_D - SCREW_D) / 2
    hole = cq.Workplane("XY").center(x_c, hole_y).circle(SCREW_D / 2).extrude(EAR_T + 2).translate((0, 0, -FLOOR - 1))
    cone = cq.Workplane("XY").add(cq.Solid.makeCone(SCREW_D / 2, SCREW_HEAD_D / 2 + 0.3, sink + 0.3,
                                                     pnt=cq.Vector(x_c, hole_y, top - sink),
                                                     dir=cq.Vector(0, 0, 1)))
    return ear, hole.union(cone)


def make_box():
    body = (
        cq.Workplane("XY")
        .box(OUT_L, OUT_W, FLOOR + INNER_H, centered=False)
        .translate((-WALL, -WALL, -FLOOR))
        .edges("|Z").fillet(CORNER_R)
    )
    body = body.cut(block(0, INNER_L, 0, INNER_W, 0, INNER_H + 1))

    # screw ears
    for y_sign in (-1, 1):
        ear, hole = screw_ear(y_sign)
        body = body.union(ear).cut(hole)

    # SMA U-slot through the +X wall (open to the top; the lid tongue closes it)
    body = body.cut(cyl_x(SMA_Y, SMA_Z, SMA_R, INNER_L - 1, INNER_L + WALL + 1))
    body = body.cut(block(INNER_L - 1, INNER_L + WALL + 1, SMA_Y - SMA_R, SMA_Y + SMA_R, SMA_Z, INNER_H + 1))

    # USB-C opening through the -X wall
    usb = (
        cq.Workplane("YZ").workplane(offset=-WALL - 1)
        .center(USB_Y, USB_Z).rect(USB_OPEN_W, USB_OPEN_H).extrude(WALL + 2)
        .edges("|X").fillet(USB_OPEN_R)
    )
    body = body.cut(usb)

    # supports: E07 on two long rails (its header pins sit in the middle),
    # ESP on two short rails (its pin rows run along the long edges)
    body = body.union(block(E07_X0, E07_X1, E07_Y0, E07_Y0 + RAIL_W, 0, SUPPORT_H))
    body = body.union(block(E07_X0, E07_X1, E07_Y0 + E07_W - RAIL_W, E07_Y0 + E07_W, 0, SUPPORT_H))
    body = body.union(block(ESP_X0, ESP_X0 + 2.0, ESP_Y0, ESP_Y0 + ESP_W, 0, SUPPORT_H))
    body = body.union(block(ESP_X1 - 2.0, ESP_X1, ESP_Y0, ESP_Y0 + ESP_W, 0, SUPPORT_H))

    # corner guides, trimmed to the cavity so they never block the openings
    cavity = block(0, INNER_L, 0, INNER_W, 0, INNER_H)
    for g in (board_guides(E07_X0, E07_Y0, E07_L, E07_W, E07_PCB),
              board_guides(ESP_X0, ESP_Y0, ESP_L, ESP_W, ESP_PCB)):
        body = body.union(g.intersect(cavity))

    # snap grooves in the inner long walls, just below the rim
    for y_wall in (0.0, INNER_W):
        body = body.cut(cyl_x(y_wall, INNER_H - SNAP_Z, GROOVE_R, SNAP_END, INNER_L - SNAP_END))
    return body


def make_lid():
    """Lid in assembled position (plate resting on the rim)."""
    lid = (
        cq.Workplane("XY")
        .box(OUT_L, OUT_W, LID_T, centered=False)
        .translate((-WALL, -WALL, INNER_H))
        .edges("|Z").fillet(CORNER_R)
    )
    x0, x1 = LIP_CLR, INNER_L - LIP_CLR
    y0, y1 = LIP_CLR, INNER_W - LIP_CLR
    lip = block(x0, x1, y0, y1, INNER_H - LIP_H, INNER_H).cut(
        block(x0 + LIP_T, x1 - LIP_T, y0 + LIP_T, y1 - LIP_T, INNER_H - LIP_H - 1, INNER_H + 1)
    )
    lid = lid.union(lip)

    # snap ridges on the lip's long faces
    for y_face in (y0, y1):
        lid = lid.union(cyl_x(y_face, INNER_H - SNAP_Z, SNAP_R, SNAP_END + 1, INNER_L - SNAP_END - 1))

    # tongue that closes the SMA slot above the connector
    tongue = block(INNER_L + 0.1, INNER_L + WALL, SMA_Y - SMA_R + 0.15, SMA_Y + SMA_R - 0.15, SMA_Z, INNER_H)
    tongue = tongue.cut(cyl_x(SMA_Y, SMA_Z, SMA_R + 0.1, INNER_L - 1, INNER_L + WALL + 1))
    return lid.union(tongue)


PLATE_GAP = 10.0      # distance between the parts on the print bed


def write_3mf(path, parts, tolerance=0.02, angular_tolerance=0.1):
    """Write a 3MF with one independent object per (name, Workplane) part.

    CadQuery's own 3MF writer merges everything into a single composite
    object, which slicers then refuse to move apart.
    """
    objects, items = [], []
    for oid, (name, part) in enumerate(parts, start=1):
        verts, tris = part.val().tessellate(tolerance, angular_tolerance)
        v = "".join(f'<vertex x="{p.x:.4f}" y="{p.y:.4f}" z="{p.z:.4f}"/>' for p in verts)
        t = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in tris)
        objects.append(f'<object id="{oid}" name="{escape(name)}" type="model">'
                       f'<mesh><vertices>{v}</vertices><triangles>{t}</triangles></mesh></object>')
        items.append(f'<item objectid="{oid}"/>')
    model = ('<?xml version="1.0" encoding="UTF-8"?>'
             '<model unit="millimeter" xml:lang="en-US" '
             'xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             f'<resources>{"".join(objects)}</resources><build>{"".join(items)}</build></model>')
    content_types = ('<?xml version="1.0" encoding="UTF-8"?>'
                     '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                     '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
                     '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
                     '</Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" '
            'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", content_types)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)


def on_bed(part, x_min=None, y_center=None):
    """Drop a part onto z = 0 and optionally place it by min X / centre Y."""
    bb = part.val().BoundingBox()
    dx = 0.0 if x_min is None else x_min - bb.xmin
    dy = 0.0 if y_center is None else y_center - (bb.ymin + bb.ymax) / 2
    return part.translate((dx, dy, -bb.zmin))


def make_print_plate(box, lid):
    """Both parts in print orientation side by side: box open side up, lid plate down."""
    box_p = on_bed(box, x_min=0.0, y_center=0.0)
    lid_flipped = lid.rotate((0, 0, 0), (1, 0, 0), 180)
    lid_p = on_bed(lid_flipped, x_min=box_p.val().BoundingBox().xmax + PLATE_GAP, y_center=0.0)
    return box_p, lid_p


def main():
    out = Path(__file__).resolve().parent
    box, lid = make_box(), make_lid()
    box_p, lid_p = make_print_plate(box, lid)
    for name, part in (("nartis_box", box_p), ("nartis_lid", lid_p)):
        cq.exporters.export(part, str(out / f"{name}.stl"), tolerance=0.02, angularTolerance=0.1)
        cq.exporters.export(part, str(out / f"{name}.step"))

    # one print scene with both parts: 3MF keeps them as two objects, STL merges them
    write_3mf(out / "nartis_print_plate.3mf", [("nartis_box", box_p), ("nartis_lid", lid_p)])
    plate = cq.Assembly(name="nartis_print_plate")
    plate.add(box_p, name="nartis_box", color=cq.Color(0.85, 0.85, 0.85))
    plate.add(lid_p, name="nartis_lid", color=cq.Color(0.5, 0.7, 0.5))
    plate.export(str(out / "nartis_print_plate.step"))
    both = cq.Workplane("XY").add(box_p.val()).add(lid_p.val())
    cq.exporters.export(both, str(out / "nartis_print_plate.stl"), tolerance=0.02, angularTolerance=0.1)
    bb_box, bb_lid = box_p.val().BoundingBox(), lid_p.val().BoundingBox()
    print(f"print plate: {bb_lid.xmax - bb_box.xmin:.1f} x {max(bb_box.ymax, bb_lid.ymax) - min(bb_box.ymin, bb_lid.ymin):.1f} mm, "
          f"height {max(bb_box.zmax, bb_lid.zmax):.1f} mm")
    print(f"closed body: {OUT_L:.1f} x {OUT_W:.1f} x {FLOOR + INNER_H + LID_T:.1f} mm (ears add {EAR_OUT:.0f} mm per side)")
    print(f"cavity:      {INNER_L:.1f} x {INNER_W:.1f} x {INNER_H:.1f} mm")
    print(f"box valid: {box.val().isValid()}, solids: {len(box.solids().vals())}; "
          f"lid valid: {lid.val().isValid()}, solids: {len(lid.solids().vals())}")
    return box, lid


if __name__ == "__main__":
    main()
