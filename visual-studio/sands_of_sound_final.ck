// OM - TIME VARYING FREQUENCY - TWINKLE TWINKLE

//-------------------------------------------------------------------
// start with the holy sound of Aum/Om
//-------------------------------------------------------------------
me.sourceDir() + "/om.wav" => string filename;
if( me.args() ) me.arg(0) => filename;

// the patch 
SndBuf buf => dac;
// load the file
filename => buf.read;

// time loop
repeat(4)
{
    0 => buf.pos;
    Math.random2f(.2,.4) => buf.gain;
    Math.random2f(.5,1.2) => buf.rate;
    6000::ms => now;
}
//-------------------------------------------------------------------

//-------------------------------------------------------------------
// Time varying frequency to leverage the beauty of the visualizer
//-------------------------------------------------------------------
SinOsc s1 => JCRev r1 => dac;
.2 => s1.gain;
.1 => r1.mix;

// an array
[ 0, 2, 4, 6, 8, 10 , 8 , 6 , 4 , 2 , 0 ] @=> int hi[];

//repeat( 4 )

for(0 => int i; i<hi.size(); i++){
    {
    for (0 => int j; j < 10 ; j++){
        Std.mtof( 45 + Math.random2(0,3) * 12 +
            hi[Math.random2(0,hi.cap()-1)] ) => s1.freq;
        100::ms => now;
        }
    }
}
0.001 => s1.gain; // rest
//-------------------------------------------------------------------


//-------------------------------------------------------------------
// Markov Chain twinkle twinkle little star - Zen Mode!
//-------------------------------------------------------------------
//durations and stuff
2::second => dur T;
T - (now % T) => now;
0::T => dur barSum;


//radnomize starting note
Std.rand2(0,5) => int currentNoteNode;
/*Std.rand2(0,3)*/ 1 => int currentRythmNode;

//patch
JCRev r;
Modulate m;	
Rhodey s => r => dac;

//initialize sound patch
.07 => s.gain;
0.6 => r.mix;


//scale to choose from
[0, 2, 4, 5, 7, 9] @=> int scale[];

//the markovchain for twinkle twinkle
 [[.5, .0, .0, .0, .5, .0],
 [.5, 1.0/3.0, .0, .0, 1.0/6.0, .0],
 [.0, .5, .5, .0, .0, .0],
 [.0, .0, .5, .5, .0, .0],
 [.0, .0, .0, .25, .5, .25],
 [.0, .0, .0, .0, .5, .5]
 ] @=> float noteGraph[][];
 
 [[.0, 1.0],
 [1.0/6.0, 5.0/6.0]] @=> float rythmgraph[][];

repeat(40){
	
	Std.fabs(Std.randf()) => float rand1;

	0.0 => float probsum1;
	//markov note
	for(0 => int i; i<noteGraph[0].cap(); i++){
		probsum1 + noteGraph[currentNoteNode][i] => probsum1;
		if(rand1 < probsum1 ){
//			<<<currentNoteNode, i>>>;
			i => currentNoteNode;
			Std.mtof(48 + scale[i]) => s.freq;
			break;
		}
	}
	
	Std.fabs(Std.randf()) => float rand2;

	0.0 => float probsum2;
	
	for(0 => int i; i< rythmgraph[0].cap(); i++){
		probsum2 + rythmgraph[currentRythmNode][i] => probsum2;
		if(rand2 < probsum2){
			i => currentRythmNode;
			T/((i+1)*2) => dur beat;
			if(barSum % T == 0::T){
				1 => s.noteOn;
//				<<< "beat one" >>>;	
			}else{
				Math.random2f( 0.3, 0.4 ) => s.noteOn;
			}
//			<<<"advanced with: ", "1/"+((i+1)*2)>>>;
			beat + barSum => barSum; 
			beat => now;
			break;
		}
	}
    
}
