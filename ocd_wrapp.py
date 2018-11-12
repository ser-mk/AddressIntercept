from ocd_rpc import OpenOcd

class OCDWrapp():
	__WORD_SIZE = 4
	__HALF_WORD_SIZE = 2
	__BYTE_SIZE = 1

	__WORD_MASK = 0xFFFFFFFF
	__HALF_WORD_MASK = 0xFFFF
	__BYTE_MASK = 0xFF

	__ADDR_MASK = 0xFFFFFFFFFFFFFFFF

	def __init__(self, ocd: OpenOcd):
		self.ocd = ocd;

	def load(addr: int, size: int) -> int:
		addr &= __ADDR_MASK
		tmp = self.ocd.readVariable(addr)
		if size == __WORD_SIZE :
			tmp &= __WORD_MASK
		elif size == __HALF_WORD_SIZE :
			tmp &= __HALF_WORD_MASK
		elif size == __BYTE_SIZE:
			tmp &= __BYTE_MASK
		else :
			tmp = None
			print("error load size!")
		return tmp

	def store(addr: int, size: int, value: int) -> bool:
		if size == __WORD_SIZE :
			value &= __WORD_MASK
			self.ocd.writeVariable(addr, value)

		read = self.ocd.readVariable(addr)
		tmp = 0
		if size == __HALF_WORD_SIZE :
			tmp = (read & (__WORD_MASK ^ __HALF_WORD_MASK)) | value # read & 0xFFFF 0000
		elif size == __BYTE_SIZE:
			tmp = (read & (__WORD_MASK ^ __BYTE_MASK)) | value # read & 0xFFFF FF00
		else :
			print("error store size!")
			return False

		ocd.writeVariable(addr, tmp)
		return True
