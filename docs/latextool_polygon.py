from latextool_basic import IFELSE
def polygon(points,
            background=None, # if background if None, no fill
            linewidth=None,
            linecolor=None,
            ):
  if points[-1] != points[0]: points.append(points[0])
  points_str = " -- ".join(["(%s,%s)" % (x,y) for (x,y) in points])
  interior = IFELSE(background, "fill=%s" % background, '')
  boundary = ''
  if linewidth or linecolor:
    if not linecolor: linecolor='black'
    if not linewidth: linewidth=0.02
    boundary = 'draw=%s, line width=%scm' % (linecolor, linewidth)
  return r"\path [%s, %s] %s;" % (interior, boundary, points_str)
