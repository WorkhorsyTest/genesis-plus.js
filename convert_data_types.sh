#!/bin/bash


shopt -s globstar
for file in source/**
do

		#INLINE
		sed -i s/INLINE\ //g $file

		#(void)
		sed -i s/\(void\)/\(\)/g $file

		# unsigned
		sed -i s/unsigned\ char/u8/g $file
		sed -i s/unsigned\ short/u16/g $file
		sed -i s/unsigned\ int/u32/g $file
		sed -i s/unsigned\ long\ long/u64/g $file

		# signed
		sed -i s/signed\ char/s8/g $file
		sed -i s/signed\ short/s16/g $file
		sed -i s/signed\ int/s32/g $file
		sed -i s/signed\ long\ long/s64/g $file

		# uint
		sed -i s/uint8/u8/g $file
		sed -i s/uint16/u16/g $file
		sed -i s/uint32/u32/g $file
		sed -i s/uint64/u64/g $file

		# sint
		sed -i s/sint8/s8/g $file
		sed -i s/sint16/s16/g $file
		sed -i s/sint32/s32/g $file
		sed -i s/sint64/s64/g $file

		# int
		sed -i s/int8/s8/g $file
		sed -i s/int16/s16/g $file
		sed -i s/int32/s32/g $file
		sed -i s/int64/s64/g $file

		# Uint
		sed -i s/Uint8/u8/g $file
		sed -i s/Uint16/u16/g $file
		sed -i s/Uint32/u32/g $file
		sed -i s/Uint64/u64/g $file

		# UINT
		sed -i s/UINT8/u8/g $file
		sed -i s/UINT16/u16/g $file
		sed -i s/UINT32/u32/g $file
		sed -i s/UINT64/u64/g $file

		#SINT
		sed -i s/SINT8/s8/g $file
		sed -i s/SINT16/s16/g $file
		sed -i s/SINT32/s32/g $file
		sed -i s/SINT64/s64/g $file

		#INT
		sed -i s/INT8/s8/g $file
		sed -i s/INT16/s16/g $file
		sed -i s/INT32/s32/g $file
		sed -i s/INT64/s64/g $file

done

