import re

text = """
white 1x, 52038: Brick, Modified 2 x 4 - 1 x 4 with 2 Recessed Studs and Thick Side Arches
white 2x, 3660: Slope, Inverted 45 2 x 2 with Flat Bottom Pin
white 4x, dark grey 1x, 3022: Plate 2 x 2
white 8x, 3023: Plate 1 x 2
white 1x, 61406pb07: Plate, Modified 1 x 2 with Angular Extension with Molded Flexible Rubber White Tip Pattern
white 4x, dark grey 11x, 3710: Plate 1 x 4
white 2x, 2431: Tile 1 x 4
dark grey 4x, 3020: Plate 2 x 4
transparent light blue 2x, 4073: Plate, Round 1 x 1
dark grey 1x, 43719: Wedge, Plate 4 x 4
dark grey 4x, 93273: Slope, Curved 4 x 1 x 2/3 Double
dark grey 8x 60478: Plate, Modified 1 x 2 with Bar Handle on End
white 3x, dark grey 1x, 85984: Slope 30 1 x 2 x 2/3
white 4x, 11090: Bar Holder with Clip
dark grey 2x, 61678: Slope, Curved 4 x 1
dark grey 4x, 15573: Plate, Modified 1 x 2 with 1 Stud with Groove and Bottom Stud Holder (Jumper)
white 4x, dark grey 2x, 15712: Tile, Modified 1 x 1 with Open O Clip
dark grey 3x, 63868: Plate, Modified 1 x 2 with Clip on End (Horizontal Grip)
dark grey 2x, 44301b: Hinge Plate 1 x 2 Locking with 1 Finger on End without Bottom Groove
dark grey 2x, 54657: Hinge Plate 1 x 2 Locking with 2 Fingers on End
dark grey 1x, 98282: Vehicle, Mudguard 4 x 2 1/2 x 1 with Arch Round
dark grey 1x, 3069: Tile 1 x 2
dark grey 1x, 48366: Plate, Modified 1 x 2 with Bar Handle on Side - Closed Ends
dark grey 1x, 2432: Tile, Modified 1 x 2 with Bar Handle
white 2x, dark grey 2x, 11477: Slope, Curved 2 x 1 x 2/3
white 2x, 3062: Brick, Round 1 x 1
white 2x, dark grey 2x, 4589b: Cone 1 x 1 with Top Groove
white 2x, 20482: Tile, Round 1 x 1 with Bar and Small Pin Hole
dark grey 2x, 32028: Plate, Modified 1 x 2 with Door Rail
dark grey 4x, 2412a: Tile, Modified 1 x 2 Grille without Bottom Groove
dark grey 3x, 35480: Plate, Round 1 x 2 with Open Studs
dark grey 3x, 61252: Plate, Modified 1 x 1 with Open O Clip (Horizontal Grip)
dark grey 2x, 4085d: Plate, Modified 1 x 1 with Clip Vertical
dark grey 2x, 99780: Bracket 1 x 2 - 1 x 2 Inverted
dark grey 2x, 3068: Tile 2 x 2
dark grey 2x, 2877: Brick, Modified 1 x 2 with Grille / Fluted Profile
"""

counts = [int(n) for n in re.findall(r'(\d+)x', text)]
total = sum(counts)

print("Individual counts:", counts)
print("Total pieces:", total)
