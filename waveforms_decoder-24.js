// rgData: input, raw digital sample array
// rgValue: output, decoded data array
// rgFlag: output, decoded flag array

const FLAG_BYTE = 1;
const FLAG_DEBUG = 2;
const FLAG_RGB = 3;
//Used to be able to alternate repeating values
const FLAG_ALTERNATE = 0x100;
const FLAG_MASK = 0xFF; 

const low_l_ns = 150;
const low_r_ns = 350;

const high_l_ns = 480;
const high_r_ns = 680;

const reset_micros = 50;


const cSamples = rgData.length;


const low_l = low_l_ns * hzRate / 1e9;
const low_r = low_r_ns * hzRate / 1e9;
const high_l = high_l_ns * hzRate / 1e9;
const high_r = high_r_ns * hzRate / 1e9;
const reset_l = reset_micros * hzRate / 1e6;


var pos_flank=-1;
var neg_flank=-1;
var bit_count=0;
var byte=-1;
var ps;
var last_data_start=-1;

var alternate=false;

function reset_data(pos) {
    bit_count = 0;
    byte = 0;
    last_data_start = pos;
}

function add_word(pos, value) {
    for (var j=last_data_start; j<=pos; j++) {
        rgValue[j] = value;
        if (alternate) {
            rgFlag[j] = FLAG_RGB | FLAG_ALTERNATE;
        } else {
            rgFlag[j] = FLAG_RGB;
        }
        
    }

    alternate = !alternate;
    last_data_start = pos;
    bit_count = 0;
    byte = 0;    
}

function add_bit(pos, bit) {
    if ((bit_count >=0) && (bit_count <= 23)) {
        if (bit) {
            byte |= (1 << (23-bit_count));
        }
        bit_count++;

        if (bit_count>23) {
            add_word(pos, byte);
        }
    }
}

for (var i=0; i<cSamples; i++) {
    var s = rgData[i] & 0x1;

    if (i==0) {
        ps = s;
    } else {


        if (s && !ps) { //Positive flank

            pos_flank = i;
            if (neg_flank != -1) {
                var width = pos_flank - neg_flank;
                if (width >= reset_l) {                    
                    reset_data(i);
                }
            } else {
                //If this is the first flank, we also treat as reset
                reset_data(i);
            }

        } else if (ps && !s) { //Negative flank
            neg_flank = i;
            if (pos_flank != -1) {
                var width = neg_flank - pos_flank;
                
                if ((width >= low_l) && (width <= low_r)) {
                    add_bit(i, false);
                } else if ((width >= high_l) && (width <= high_r)) {
                    add_bit(i, true);
                }


            }

        }

        ps = s;

    }

}