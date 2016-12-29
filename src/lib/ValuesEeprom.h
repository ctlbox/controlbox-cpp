/*
 * Copyright 2014-2015 Matthew McGowan.
 *
 * This file is part of Nice Firmware.
 *
 * Controlbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Controlbox.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "DataStream.h"
#include "Values.h"
#include "EepromAccess.h"
#include "DataStreamEeprom.h"
#include "StreamUtil.h"
#include "Static.h"

/**
 * Base class for a read-write value in eeprom. This class is responsible for moving the data
 * between eeprom and the stream.
 */
class EepromBaseValue : public WritableValue  {

protected:
#if !CONTROLBOX_STATIC
	EepromAccess& eepromAccess;

	EepromBaseValue(EepromAccess& ea) : eepromAccess(ea){}
#endif


	void _readTo(DataOut& out, eptr_t offset, uint8_t size)
	{
		EepromDataIn in cb_nonstatic_decl((eepromAccess));
		in.reset(offset, size);
		in.push(out, size);
	}

	void _writeFrom(DataIn& in, eptr_t offset, uint8_t size)
	{
		EepromDataOut out cb_nonstatic_decl((eepromAccess));
		out.reset(offset, size);
		in.push(out, size);
	}

	/**
	 * Writes masked data to eeprom starting at the given address.
	 */
	void _writeMaskedFrom(DataIn& dataIn, DataIn& maskIn, int8_t length,
                                                        eptr_t address) {
		while (--length>=0) {
			uint8_t current = eepromAccess.readByte(address);
			uint8_t update = WritableValue::nextMaskedByte(current, dataIn, maskIn);
			eepromAccess.writeByte(address++, update);
		}
	}

	void _writeMaskedOut(DataIn& dataIn, DataIn& maskIn, DataIn& in, DataOut& out, int8_t length) {
		while (--length>=0) {
			out.write(WritableValue::nextMaskedByte(in.next(), dataIn, maskIn));
		}
	}


	object_t objectType() { return ObjectFlags::Value | ObjectFlags::WritableFlag; }

};

/**
 * An object that uses it's construction definition in eeprom storage also as it's value.
 * So, the initial construction data block sets the desired size and initial data.
 * Subsequent read/write operations read and write to that data.
 */
class EepromValue : public EepromBaseValue
{
protected:
        eptr_t address;

public:

    cb_nonstatic_decl(EepromValue(EepromAccess& ea):EepromBaseValue(ea){})

	void rehydrated(eptr_t address)
	{
		this->address = address;
	}

	void readTo(DataOut& out) {
		_readTo(out, eeprom_offset(), EepromValue::readStreamSize());
	}

	void writeMaskedFrom(DataIn& dataIn, DataIn& maskIn) {
		_writeMaskedFrom(dataIn, maskIn, EepromValue::writeStreamSize(), address);
	}

	eptr_t eeprom_offset() { return address; }
	uint8_t readStreamSize() { return eepromAccess.readByte(address-1); }

	static Object* create(ObjectDefinition& defn)
	{
		return new_object(EepromValue(cb_nonstatic_decl(defn.eepromAccess())));
	}
};


#if 0
/**
 * Provides state read/write (in addition to stream read/write) for an eeprom value.
 */
template <class T, int _size=sizeof(T)> class EepromValue : public EepromStreamValue
{
	public:

		EepromValue(eptr_t offset) : EepromStreamValue(offset) {}

		T read() {
			T result;
			EepromDataIn in(this->_offset, _size);
			in.read((uint8_t*)&result, _size);
			return result;
		}

		void write(T t) {
			EepromDataOut out(this->_offset, _size);
			out.write(&t, _size);
		}

		uint8_t streamSize() { return _size; }
};


/**
 * Provides a streamable value to eeprom. The size is dynamic, unlike EepromStreamValue, where the size is
 * known at compile time.
 * mdm: I forget why I wrote this - it's presently unused.
 */
class EepromDynamicStreamValue : public EepromBaseValue
{
	private:
		eptr_t _offset;
		uint8_t _size;

	public:
		EepromDynamicStreamValue(eptr_t offset, uint8_t size) : _offset(offset), _size(size) {}

		void writeFrom(DataIn& in) {
			_writeFrom(in, _offset, _size);
		}

		void readTo(DataOut& out) {
			_readTo(out, _offset, _size);
		}

		uint8_t streamSize() { return _size; }
};
#endif
