#!/usr/bin/env python3.5m
# -*- coding: utf-8 -*-

from scanf import scanf

from pprint import pprint


PATTERN_SCANF = "id:%d | %s | addr: %x | size: %d | value: %x"
PATTERN_SPRINTF = "id:%d | %s | addr: 0x%x | size: %d | value: 0x%x"

COMMAND_LOAD = "LOAD"
COMMAND_STORE = "STORE"
COMMAND_NOP = "NOP!"


class AddrCommand:
	_id = -1
	command = COMMAND_NOP
	addr = -1
	size = -1
	value = -1


def __parse(line):
	struct = AddrCommand()
	try:
		_id, command, addr, size, value = scanf(PATTERN_SCANF, line)
	except Exception as e:
		print("parse - ", e, " - line : ", line)
		raise e
	else:
		struct._id = _id
		struct.command = command
		struct.addr = addr
		struct.size = size
		struct.value = value
	finally:
		return struct

def proccess(line):
	struct = __parse(line)
	print(struct._id, struct.command, struct.addr, struct.size, struct.value)

	return line
