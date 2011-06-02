//Channel.cpp
//class file for channel class
#include "Channel.h"
#include "ChannelPacket.h"
#include "Controller.h"

using namespace NVDSim;

Channel::Channel(void){
	sender = -1;
	busy = 0;
	cyclesLeft = 0;
	beatsLeft = 0;

	cyclesLeft = new uint *[DIES_PER_PACKAGE];
	beatsLeft = new uint *[DIES_PER_PACKAGE];
	beatsDone = new uint *[DIES_PER_PACKAGE];
	deviceWriting = new uint *[DIES_PER_PACKAGE];
	writePending = new uint *[DIES_PER_PACKAGE];

	for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	    cyclesLeft[i] = new uint[PLANES_PER_DIE];
	    beatsLeft[i] = new uint[PLANES_PER_DIE];
	    beatsDone[i] = new uint[PLANES_PER_DIE];
	    deviceWriting[i] = new uint[PLANES_PER_DIE];
	    writePending[i] = new uint[PLANES_PER_DIE];
	}

	firstCheck = 0;
}

void Channel::attachDie(Die *d){
	dies.push_back(d);
}

void Channel::attachController(Controller *c){
	controller= c;
}

int Channel::obtainChannel(uint s, SenderType t, ChannelPacket *p){
	if (sender != -1 || (t == CONTROLLER && dies[p->die]->isDieBusy(p->plane))){
		return 0;
	}
	type = t;
	sender = (int) s;
	return 1;
}

int Channel::releaseChannel(SenderType t, uint s){   
        // these should be zero anyway but clear then just to be safe
        cyclesLeft = 0;
	beatsLeft = 0;
    
	if (t == type && sender == (int) s){
		sender = -1;
		return 1;
	}
	return 0;
}

int Channel::hasChannel(SenderType t, uint s){
	if (t == type && sender == (int) s)
		return 1;
	return 0;
}

void Channel::sendToDie(ChannelPacket *busPacket){
	dies[busPacket->die]->receiveFromChannel(busPacket);
}

void Channel::sendToController(ChannelPacket *busPacket){
	controller->receiveFromChannel(busPacket);
}

//TODO: Need to check the type of piece to see if its a command or data
void Channel::sendPiece(SenderType t, uint type, uint die, uint plane){
        if(t == CONTROLLER){
	    if(beatsLeft[die][plane] > 0){
		beatsDone[die][plane]++;
	    }else{
		if(type == 0){
		    beatsLeft[die][plane] = divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH);
		}else{
		    beatsLeft[die][plane] = divide_params(COMMAND_LENGTH,DEVICE_WIDTH);
		}
		beatsDone[die][plane] = 1;
		busy = 1;
	    }
	}else if(t == DIE){
	    if(beatsLeft[die][plane] > 0){
		beatsLeft[die][plane] += divide_params(DEVICE_WIDTH,CHANNEL_WIDTH);
	    }else{
		cyclesLeft[die][plane] = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
		beatsLeft[die][plane] = divide_params(DEVICE_WIDTH,CHANNEL_WIDTH);
		busy = 1;
	    }
	}
}

int Channel::notBusy(void){
    if(busy == 1){
	return 0;
    }else{
	return 1;
    }
}

void Channel::update(void){
    if(type == CONTROLLER){
	for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	    for(uint j = 0; j < PLANES_PER_DIE; j++){
		if(beatsDone[i][j]%divide_params(DEVICE_WIDTH,CHANNEL_WIDTH) == 0 && 
		   beatsDone[i][j] > 0 && deviceWriting[i][j] == 0)
		{
		    cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    deviceWriting[i][j] = 1;
		}
		if(deviceWriting[i][j] == 1)
		{
		    cout << cyclesLeft[i][j] << "\n";
		    cyclesLeft[i][j]--;
		    if(cyclesLeft[i][j] <= 0)
		    {
			deviceWriting[i][j] = 0;
			beatsLeft[i][j]--;
		    }	
		}
		if(busy == 1 && beatsDone[i][j] == divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH)
		    && writePending[i][j] == 0)
		{
		    cout << "we keep getting here \n";
		    cout << "current clock cycle is " << dies[0]->currentClockCycle << "\n";
		    cout << "beatsDone is " << beatsDone[i][j] << "it should be " << divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH) << "\n";
		    cout << "beatsLeft is " << beatsLeft[i][j] << "\n";
		    cout << "busy is " << busy << "\n";
		    writePending[i][j] = 1;
		    busy = 2;
		}
		if(beatsLeft[i][j] <= 0 && writePending[i][j] == 1){
		    cout << "shouldn't see this more than a few times \n";
		    controller->channelDone(i,j);
		    writePending[i][j] = 0;
		    beatsDone[i][j] = 0;
		}
	    }
	}	
    }else if(type == DIE){
	for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	    for(uint j = 0; j < PLANES_PER_DIE; j++){
		cyclesLeft[i][j]--;
		if(cyclesLeft[i][j] <= 0 && beatsLeft[i][j] > 0){
		    beatsLeft[i][j]--;
		    cyclesLeft[i][j] = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
		}
		if(beatsLeft[i][j] <= 0){
		    busy = 2;
		}
	    }
	}
    }
}
