#!/usr/bin/env python3.5m
# -*- coding: utf-8 -*-

"""
Covered by Apache License 2.0
Copyright (C) 2018 Evgeny Chormonov (en.chormonov@gmail.com)
For more information https://github.com/ser-mk/AddressIntercept
"""

from scanf import scanf
from ocd_rpc import OpenOcd
from ocd_wrapp import OCDWrapp

import logging


PATTERN_SCANF = "id:%d | %s | addr: %x | size: %d | value: %x | st: "
PATTERN_SPRINTF = "id:%d | %s | addr: 0x%x | size: %d | value: 0x%x | st: %s\n"

COMMAND_LOAD = "LOAD"
COMMAND_STORE = "STORE"
COMMAND_NOP = "NOP!"

NO_ERROR_STATUS = "NO_ERROR"
OCD_ERROR_STATUS = "OCD_ERROR"


_ocdw = None


def initOCD(ocd: OpenOcd) -> None:
    global _ocdw
    _ocdw = OCDWrapp(ocd)
    ocd.send("reset halt")
    logging.info("connect OCD server")


class AddrCommand:
    _id = -1
    command = None
    addr = -1
    size = -1
    value = -1
    status = NO_ERROR_STATUS


def _checkValueSuccess(value):
    if type(value) is int:
        if value >= 0:
            return True
    return False


def _load(struct_command: AddrCommand) -> int:
    global _ocdw
    value = -1
    try:
        value = _ocdw.load(struct_command.addr, struct_command.size)
    except Exception as e:
        # raise
        # struct_command.status = OCD_ERROR_STATUS
        logging.critical(e)
    else:
        if not _checkValueSuccess(value):
            # struct_command.status = OCD_ERROR_STATUS
            value = -1
    return value


def _store(struct_command: AddrCommand) -> bool:
    global _ocdw
    success = False
    try:
        success = _ocdw.store(struct_command.addr,
                              struct_command.size, struct_command.value)
    except Exception as e:
        # raise
        # struct_command.status = OCD_ERROR_STATUS
        logging.critical(e)

    return success


def _createLine(struct_command: AddrCommand) -> str:
    tmp = PATTERN_SPRINTF % (struct_command._id, struct_command.command, struct_command.addr,
                             struct_command.size, struct_command.value, struct_command.status)
    return tmp


def _parse(line: str) -> AddrCommand:
    struct = AddrCommand()
    try:
        _id, command, addr, size, value = scanf(PATTERN_SCANF, line)
    except Exception as e:
        logging.error("parse - %s - line : %s", e, line)
        raise e
    else:
        struct._id = _id
        struct.command = command
        struct.addr = addr
        struct.size = size
        struct.value = value
    finally:
        return struct


def proccess(line: str) -> str:
    # todo: check size of line
    struct_command = _parse(line)
    logging.debug("struct_command: %d %s %x %d %x %s", struct_command._id, struct_command.command,
                  struct_command.addr, struct_command.size, struct_command.value, struct_command.status)
    # todo: check id of message
    if struct_command.command == COMMAND_LOAD:
        value = _load(struct_command)
        if value < 0:
            struct_command.status = OCD_ERROR_STATUS
        else:
            struct_command.value = value
    elif struct_command.command == COMMAND_STORE:
        if not _store(struct_command):
            struct_command.status = OCD_ERROR_STATUS

    return _createLine(struct_command)
