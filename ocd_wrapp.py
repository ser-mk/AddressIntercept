from ocd_rpc import OpenOcd
import logging


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

	def load(self, addr: int, size: int) -> int:
		addr &= self.__ADDR_MASK
		tmp = self.ocd.readVariable(addr)
		if size == self.__WORD_SIZE :
			tmp &= self.__WORD_MASK
		elif size == self.__HALF_WORD_SIZE :
			tmp &= self.__HALF_WORD_MASK
		elif size == self.__BYTE_SIZE:
			tmp &= self.__BYTE_MASK
		else :
			tmp = None
			logging.error("error load size!")
		return tmp

	def store(self, addr: int, size: int, value: int) -> bool:
		if size == self.__WORD_SIZE :
			value &= self.__WORD_MASK
			self.ocd.writeVariable(addr, value)
			return True

		read = self.ocd.readVariable(addr)
		tmp = 0
		if size == self.__HALF_WORD_SIZE :
			tmp = (read & (self.__WORD_MASK ^ self.__HALF_WORD_MASK)) | value # read & 0xFFFF 0000
		elif size == self.__BYTE_SIZE:
			tmp = (read & (self.__WORD_MASK ^ self.__BYTE_MASK)) | value # read & 0xFFFF FF00
		elif size != self.__WORD_SIZE :
			logging.error("error store size!")
			return False

		logging.debug("store: addr %x tmp %x", addr, tmp)
		self.ocd.writeVariable(addr, tmp)
		return True
