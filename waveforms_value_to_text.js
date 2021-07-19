const FLAG_BYTE = 1;
const FLAG_DEBUG = 2;
const FLAG_GRB = 3;
//Used to be able to alternate repeating values
const FLAG_ALTERNATE = 0x100;
const FLAG_MASK = 0xFF; 
// value: value sample
// flag: flag sample

//Note - template strings not supported
//Note big endian

function hex(value) {
	const ha = "0123456789ABCDEF";
	return ha[value >> 4] + ha[value & 0xF];
}

function Value2Text(flag, value){
  switch(flag & FLAG_MASK){
    case FLAG_BYTE: return "0x" + hex(value);
    case FLAG_DEBUG: return "DBG: " + value;
    case FLAG_GRB: return "rgb(" + ((value >> 8) & 0xFF) + ", " + ((value >> 16) & 0xFF) + ", " + (value & 0xFF) +")";
    default: return "???";
  }
}