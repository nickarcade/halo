"""Independent synthetic cases for the retail comparison's evidence limits."""
import unittest

try:
    from tools.audit import retail_migration_pilot as pilot
except ModuleNotFoundError as error:
    if error.name != 'capstone':
        raise
    pilot = None


@unittest.skipIf(pilot is None, 'Capstone is a local research dependency')
class RetailComparisonEvidenceTest(unittest.TestCase):
    def signature(self, data, start=0x1000, sections=None):
        sections = sections or {'.text': (start, data, len(data))}
        instructions = pilot.decode(sections, start, len(data), require_complete=True)
        return pilot.normalize(sections, instructions, start, len(data))

    def test_arithmetic_constants_are_not_normalized_as_addresses(self):
        one = bytes.fromhex('83c001c3')  # Independently constructed ADD EAX,1; RET.
        two = bytes.fromhex('83c002c3')
        self.assertNotEqual(self.signature(one)[0], self.signature(two)[0])

    def test_relative_internal_branches_survive_address_relocation(self):
        data = bytes.fromhex('eb0090c3')  # JMP to this function's NOP; RET.
        self.assertEqual(self.signature(data, 0x1000)[0],
                         self.signature(data, 0x3000)[0])

    def test_global_addresses_are_recorded_without_proving_pointees(self):
        data = bytes.fromhex('a100400000c3')  # MOV EAX,[0x4000]; RET.
        sections = {'.text': (0x1000, data, len(data)), '.data': (0x4000, b'ABCD', 4)}
        signature = self.signature(data, sections=sections)
        self.assertEqual(signature[2], [(0, 0x4000, 4, 'memory')])
        self.assertIn('absolute_address', str(signature[0]))

    def test_incomplete_decoder_prefix_is_not_a_complete_body(self):
        data = bytes.fromhex('c30f')  # RET followed by an incomplete instruction.
        sections = {'.text': (0x1000, data, len(data))}
        self.assertEqual(pilot.decode(sections, 0x1000, 2, require_complete=True), [])

    def test_access_width_changes_are_not_normalized_away(self):
        byte_load = bytes.fromhex('8a01c3')
        dword_load = bytes.fromhex('8b01c3')
        self.assertNotEqual(self.signature(byte_load)[0], self.signature(dword_load)[0])


if __name__ == '__main__':
    unittest.main()
