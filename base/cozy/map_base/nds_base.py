# -*- coding: UTF-8 -*-
"""
* tile 相关的标准方法.
"""
# convert wgs84 coordinate to nds coordinate
wgs84_to_nds_coordinate = lambda x: int((x / 360.0) * 4294967296)

class Tile(object):
    """
    * This class provides all functions needed for the tile scheme.
    """
    def __init__(self):
        self.max_level = 16
        self.max_tile_num_size = 31
        self.base_y = 0xC0000000
        self.base_x = 0x80000000

    #
    def get_tile_id_from_point(self, point, level):
        #
        morton_code = MortonCode(point)
        tile_id = self.get_tile_id_from_morton_code(morton_code, level)
        return tile_id

    #
    def get_tile_number_from_morton_code( self, morton_code, level):
        #
        count_bits = 2 * level + 1
        move_count = morton_code.morton_bit_count - count_bits
        morton_code_value = morton_code.get_morton_code() >> move_count

        return morton_code_value

    #
    def get_tile_id_from_morton_code(self, morton_code, level):
        #
        tile_number = self.get_tile_number_from_morton_code(morton_code, level)
        #
        return self.get_tile_id_from_tile_number(tile_number, level)

    #
    def get_tile_id_from_tile_number(self, tile_number, level):
        return tile_number | (0x01 << (self.max_level + level))

    #
class MortonCode(object):
    """
    * A class for the Morton code, it provides functions to get the
    * Morton code from coordinates or to get the tile number or tile id.
    """
    #
    def __init__(self, point):
        self.morton_bit_count = 63
        self.x = wgs84_to_nds_coordinate(point[0])
        self.y = wgs84_to_nds_coordinate(point[1])
    #
    def get_morton_code(self):
        a_m = [ 0x5555555555555555, 0x3333333333333333, 0x0F0F0F0F0F0F0F0F, 0x00FF00FF00FF00FF, 0x0000FFFF0000FFFF ]
        a_s = [ 1, 2, 4, 8, 16 ]
        x = (self.x) & 0xffffffff
        y = (self.y) & 0x7fffffff

        x = (x | (x << a_s[4])) & a_m[4]
        x = (x | (x << a_s[3])) & a_m[3]
        x = (x | (x << a_s[2])) & a_m[2]
        x = (x | (x << a_s[1])) & a_m[1]
        x = (x | (x << a_s[0])) & a_m[0]

        y = (y | (y << a_s[4])) & a_m[4]
        y = (y | (y << a_s[3])) & a_m[3]
        y = (y | (y << a_s[2])) & a_m[2]
        y = (y | (y << a_s[1])) & a_m[1]
        y = (y | (y << a_s[0])) & a_m[0]

        morton_code = x | (y << 1)
        return morton_code

"""
* 测试样例及代码.
"""
def test_get_tile_id_from_point():
    # Initialize the Tile class
    tile = Tile()

    # Test case 1: 上海市样例点 (121.75916425242, 31.27959245245243), level=13 , Result: 557040827
    point = (121.75916425242, 31.27959245245243)
    level = 13
    result = tile.get_tile_id_from_point(point, level)
    print(f"Test Case 1 - Point: {point}, Level: {level}, Result: {result}")

    # Test case 2:  北京市样例点 (116.399502345234524, 39.975932523452435), level=13, Result: 557467531
    point = (116.399502345234524, 39.975932523452435)
    level = 13
    result = tile.get_tile_id_from_point(point, level)
    print(f"Test Case 2 - Point: {point}, Level: {level}, Result: {result}")

    # Test case 3: 上海市样例点 (121.37796,31.24267), level=13, Result: 557040050
    point = (121.37796, 31.24267)
    level = 13
    result = tile.get_tile_id_from_point(point, level)
    print(f"Test Case 3 - Point: {point}, Level: {level}, Result: {result}")

    # Add more test cases as needed

if __name__ == "__main__":
    test_get_tile_id_from_point()