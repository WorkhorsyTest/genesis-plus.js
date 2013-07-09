

import std.stdio;
import std.string;

import sdl;

public import system;
public import common;
public import main;
public import config;
public import error;
public import fileio;

alias sdl_input_update osd_input_update;

const string GG_ROM      = "./ggenie.bin";
const string AR_ROM      = "./areplay.bin";
const string SK_ROM      = "./sk.bin";
const string SK_UPMEM    = "./sk2chip.bin";
const string CD_BIOS_US  = "./bios_CD_U.bin";
const string CD_BIOS_EU  = "./bios_CD_E.bin";
const string CD_BIOS_JP  = "./bios_CD_J.bin";
const string MD_BIOS     = "./bios_MD.bin";
const string MS_BIOS_US  = "./bios_U.sms";
const string MS_BIOS_EU  = "./bios_E.sms";
const string MS_BIOS_JP  = "./bios_J.sms";
const string GG_BIOS     = "./bios.gg";

