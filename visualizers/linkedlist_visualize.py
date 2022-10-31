import json
import sys
from turtle import bgcolor
import matplotlib.pyplot
import numpy
import cairo
import math

def next_power_of_2(n: int):
    if n == 0:
        return 1
    return (1 << (n - 1).bit_length())

def hex(color: str):
    if color[0] == '#':
        color = color[1:]
    if len(color) == 3:
        r = int(color[0] + color[0], base=16)
        g = int(color[1] + color[1], base=16)
        b = int(color[2] + color[2], base=16)
    elif len(color) == 6:
        r = int(color[0:2], base=16)
        g = int(color[2:4], base=16)
        b = int(color[4:6], base=16)
    else:
        raise ValueError("Unsupported hex color format")

    return (r / 255.0, g / 255.0, b / 255.0)


PADDING = (50, 150, 50, 50)
BLOCK_WIDTH = 13
ROW_HEIGHT = 20
ROW_PADDING = 2

COLOR_BG = hex('#23272e')
COLORS = [
    hex('#a5e075'),
    hex('#f14c4c'),
    hex('#61afef')
]
COLOR_EMPTY = (.1, .1, .1)

bitmap = json.load(sys.stdin)

# figure out memory size
memSize = 0
for block in bitmap['blockList']:
    memSize = max(memSize, block['base'] + block['size'])

row_width = next_power_of_2(math.floor(math.sqrt(memSize)))
row_count = math.ceil(memSize / row_width)

# generate blank image
image_width = PADDING[0] + row_width * BLOCK_WIDTH + PADDING[2]
image_height = PADDING[1] + row_count * (ROW_HEIGHT + ROW_PADDING) + PADDING[3]

surface = cairo.SVGSurface("linkedlist.svg", image_width, image_height)
cr = cairo.Context(surface)

# background
cr.set_source_rgb(*COLOR_BG)
cr.paint()

# draw darker bg
for i in range(row_count):
    w = row_width
    if i == row_count - 1:
        w = memSize % row_width
    cr.set_source_rgb(*COLOR_EMPTY)
    cr.rectangle(PADDING[0],
                 PADDING[1] + i * (ROW_HEIGHT + ROW_PADDING),
                 w * BLOCK_WIDTH,
                 ROW_HEIGHT + ROW_PADDING + 0.5)
    cr.fill()

# draw regions
for block in bitmap['blockList']:
    # only draw used blocks
    if block['type'] != 0:
        base = block['base']
        size = block['size']
        c = COLORS[block['type']]

        # draw begin thing
        row = base / row_width
        col = base % row_width
        cr.set_source_rgb(*c)
        cr.move_to(PADDING[0] + col * BLOCK_WIDTH,
                   PADDING[1] + row * (ROW_HEIGHT + ROW_PADDING))
        cr.line_to(PADDING[0] + col * BLOCK_WIDTH,
                   PADDING[1] + row * (ROW_HEIGHT + ROW_PADDING) + ROW_HEIGHT)
        cr.line_to(PADDING[0] + col * BLOCK_WIDTH + 7,
                   PADDING[1] + row * (ROW_HEIGHT + ROW_PADDING) + ROW_HEIGHT / 2)
        cr.fill()

        # draw end thing
        row = (base + size) / row_width
        col = (base + size) % row_width
        if col == 0:
            row -= 1
            col = row_width

        cr.set_source_rgb(*c)
        cr.move_to(PADDING[0] + col * BLOCK_WIDTH,
                   PADDING[1] + row * (ROW_HEIGHT + ROW_PADDING))
        cr.line_to(PADDING[0] + col * BLOCK_WIDTH,
                   PADDING[1] + row * (ROW_HEIGHT + ROW_PADDING) + ROW_HEIGHT)
        cr.line_to(PADDING[0] + col * BLOCK_WIDTH - 7,
                   PADDING[1] + row * (ROW_HEIGHT + ROW_PADDING) + ROW_HEIGHT / 2)
        cr.fill()

        while size > 0:
            row = base / row_width
            col = base % row_width
            width = min(size, row_width - col)

            cr.set_source_rgb(*c)
            cr.rectangle(PADDING[0] + col * BLOCK_WIDTH,
                         PADDING[1] + row * (ROW_HEIGHT + ROW_PADDING) + 6,
                         width * BLOCK_WIDTH,
                         ROW_HEIGHT - 12)
            cr.fill()

            base += width
            size -= width

surface.finish()
surface.flush()




#
# Generate linked list graph with graphviz
#
BLOCK_TYPES = [
    'Free',
    'Used'
]


import graphviz
dot = graphviz.Digraph(format='svg',
                       graph_attr={
                           'rankdir': 'LR',
                           'rank': 'same'
                       })

for block in bitmap['blockList']:

    label = '{ <prev> ' \
        + f' | <base> { block["base"] }' \
        + f' | <size> { block["size"] }' \
        + f' | <type> { BLOCK_TYPES[block["type"]] }' \
        + ' | <next> }'

    label = f'''<<table border="0" cellspacing="0" cellborder="1">
      <tr>
        <td port="prev" width="28" height="36" fixedsize="true"></td>
        <td port="base" width="28" height="36" fixedsize="false">{ block["base"] }</td>
        <td port="size" width="28" height="36" fixedsize="false">{ block["size"] }</td>
        <td port="type" width="28" height="36" fixedsize="false">{ BLOCK_TYPES[block["type"]] }</td>
        <td port="next" width="28" height="36" fixedsize="true"></td>
      </tr>
    </table>>'''

    dot.node(str(block['id']),
             label=label,
             shape='plaintext')

    if block['prev'] is not None:
        dot.edge('{0}:prev'.format(block['id']), 
                 str(block['prev']),
                 arrowtail='dot',
                 dir='forward')

    if block['next'] is not None:
        dot.edge('{0}:next'.format(block['id']),
                 str(block['next']),
                 arrowtail='dot',
                 dir='forward')

dot.render("linkedlist_nodes")
