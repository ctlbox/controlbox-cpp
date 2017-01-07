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

#include "pointer_scalar.h"
#include "Values.h"
#include "ValuesEeprom.h"
#include "SystemProfile.h"

class ExternalReadOnlyValue : public Value
{
protected:
	void* const _pValue;
	uint8_t const _size;
public:
	ExternalReadOnlyValue(void* pValue, uint8_t size) :
		_pValue(pValue), _size(size) {}

	void readTo(DataOut& out) {
		out.writeBuffer(_pValue, _size);
	}

	uint8_t readStreamSize() {
		return _size;
	}

};

/**
 * A streamable value whose data resizes externally.
 */
// TODO - factor ExternalValue, eeprom value and progmem value - all have address and size.
class ExternalValue : public ExternalReadOnlyValue, public WritableValue
{
public:

	ExternalValue(void* pValue, uint8_t size) : ExternalReadOnlyValue(pValue, size) {}

	void writeMaskedFrom(DataIn& in, DataIn& mask) {
		uint8_t* p = (uint8_t*)_pValue;
		for (uint8_t i=0; i<_size; i++) {
			*p = nextMaskedByte(*p, in, mask);
			p++;
		}
	}
};

template <class T> class TransientValue : public WritableValue
{
	T value;
public:

	void readTo(DataOut& out) {
		writePlatformEndianBytes(&value, sizeof(value), out);
	}

	void writeMaskedFrom(DataIn& in, DataIn& maskIn) {
		readPlatformEndianMaskedBytes(&value, sizeof(value), in, maskIn);
	}

	uint8_t readStreamSize() { return sizeof(T); }

	void setValue(const T& value) {
		this->value = value;
	}

	T getValue() { return value; }

};

/**
 * Callback function for use by this container to establish values.
 * @param id	The index (greater or equal to 0) in the container to return the value of. When negative, returns
 *   the number of items available.
 * @return an object corresponding to the item at the given index, which may be {@code NULL}.
 *   When {@code id} is negative, returns the number of items, cast as a pointer.
 */
typedef Object* (*ExternalValueFunction)(container_id id);

/**
 * Provides a container interface via values sourced externally.
 * To avoid overhead of a virtual function table and vptr which require storage for two addresses,
 * a single function is used to retrieve.
 */
class ExternalValueContainer : Container {

	ExternalValueFunction externalValue;

public:
	ExternalValueContainer(ExternalValueFunction fn) {
		externalValue = fn;
	}

	/**
	 * Returns a transient value item.
	 */
	Object* item(container_id id) override {
		return externalValue(id);
	}

	void returnItem(container_id /*id*/, Object* item) override {
#if OBJECT_VIRTUAL_DESTRUCTOR
		delete item;
#else
		delete (uint8_t*)item;	// just clear the memory
#endif
	}

	container_id size() override {
		return container_id(pointer_scalar(externalValue(-1)));
	}
};

/**
 * An indirect value is configured with the id chain of an object, and returns that object's value.
 * Once created the value pointed to cannot be changed.
 * This is used in cases where you have a complex object implemented as a container, and it requires
 * values to be inserted into it, yet the value you want to insert is already instantiated elsewhere else.
 * It behaves something like a pointer.
 */
class IndirectValue : public EepromValue
{
private:
	Value* previous;

	cb_nonstatic_decl(Container* root_;)

	void fetchTarget() {
		EepromDataIn data cb_nonstatic_decl((eepromAccess));
		data.reset(address, eepromAccess.readByte(address-1));
		previous = (Value*)lookupUserObject(root(), data);
	}
public:

#if !CONTROLBOX_STATIC
	IndirectValue(EepromAccess& ea, Container* root) : EepromValue(ea), root_(root) {}
#endif

	inline Container* root()
	{
#if CONTROLBOX_STATIC
		return systemProfile.rootContainer();
#else
		return root_;
#endif
	}

	object_t objectType() {
		fetchTarget();
		return previous==NULL ? ObjectFlags::Object : previous->objectType();
	}

	prepare_t prepare() {
		fetchTarget();
		return previous==NULL ? 0 : previous->prepare();
	}

	uint8_t readStreamSize() { return previous->readStreamSize(); }
	void readTo(DataOut& out) { if (previous) previous->readTo(out); }
	uint8_t writeStreamSize() { return ((WritableValue*)previous)->writeStreamSize(); }
	void writeMaskedFrom(DataIn& dataIn, DataIn& maskedIn) { if (previous) ((WritableValue*)previous)->writeMaskedFrom(dataIn, maskedIn); }

	static Object* create(ObjectDefinition& def)
	{
		return new_object(IndirectValue(cb_nonstatic_decl(def.eepromAccess(), def.root)));
	}
};
