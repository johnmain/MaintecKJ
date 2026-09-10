// DJControl_Inpulse_200mk2_script.js
//
// ***************************************************************************
// * Mixxx mapping script file for the Hercules DJControl Inpulse 200mk2.
// * Author: DJ Phatso, contributions by Kerrick Staley
// * Version 1.0 (August 2023)
// 
//
// * Based on Hercules DJControl Inpulse 200 mapping released with Mixxx v2.3.0
//
// *  -Remapped LOOP and VINYL section according to new DJControl 200 MK2 layout
// *  -Fix soft-takeover for Pitch sliders Deck1/Deck2
//
//*************************************************************************

var DJCi200mk2 = {};
///////////////////////////////////////////////////////////////
//                       USER OPTIONS                        //
///////////////////////////////////////////////////////////////

// How fast scratching is.
DJCi200mk2.scratchScale = 1.0;

// How much faster seeking (shift+scratch) is than scratching.
DJCi200mk2.scratchShiftMultiplier = 4;

// How fast bending is.
DJCi200mk2.bendScale = 1.0;

// Other scratch related options
DJCi200mk2.kScratchActionNone = 0;
DJCi200mk2.kScratchActionScratch = 1;
DJCi200mk2.kScratchActionSeek = 2;
DJCi200mk2.kScratchActionBend = 3;

DJCi200mk2.init = function() {
    // Scratch button state
    DJCi200mk2.scratchButtonState = true;
    // Scratch Action
    DJCi200mk2.scratchAction = {
        1: DJCi200mk2.kScratchActionNone,
        2: DJCi200mk2.kScratchActionNone
    };

    //Turn On Vinyl buttons LED(one for each deck).
    midi.sendShortMsg(0x94, 0x03, 0x7F);
    midi.sendShortMsg(0x95, 0x03, 0x7F);

    //Turn On Browser button LED
    midi.sendShortMsg(0x90, 0x04, 0x05);

    //Softtakeover for Pitch fader
    engine.softTakeover("[Channel1]", "rate", true);
    engine.softTakeover("[Channel2]", "rate", true);
    engine.softTakeoverIgnoreNextValue("[Channel1]", "rate");
    engine.softTakeoverIgnoreNextValue("[Channel2]", "rate");

    //Set effects Levels - Dry/Wet
    engine.setParameter("[EffectRack1_EffectUnit1_Effect1]", "meta", 0.6);
    engine.setParameter("[EffectRack1_EffectUnit1_Effect2]", "meta", 0.6);
    engine.setParameter("[EffectRack1_EffectUnit1_Effect3]", "meta", 0.6);
    engine.setParameter("[EffectRack1_EffectUnit2_Effect1]", "meta", 0.6);
    engine.setParameter("[EffectRack1_EffectUnit2_Effect2]", "meta", 0.6);
    engine.setParameter("[EffectRack1_EffectUnit2_Effect3]", "meta", 0.6);
    engine.setParameter("[EffectRack1_EffectUnit1]", "mix", 1);
    engine.setParameter("[EffectRack1_EffectUnit2]", "mix", 1);

    // Ask the controller to send all current knob/slider values over MIDI, which will update
    // the corresponding GUI controls in MIXXX.
    midi.sendShortMsg(0xB0, 0x7F, 0x7F);
};

// The Vinyl button, used to enable or disable scratching on the jog wheels (One per deck).
DJCi200mk2.vinylButton = function(_channel, _control, value, status, _group) {
    if (value) {
        if (DJCi200mk2.scratchButtonState) {
            DJCi200mk2.scratchButtonState = false;
            midi.sendShortMsg(status, 0x03, 0x00);
        } else {
            DJCi200mk2.scratchButtonState = true;
            midi.sendShortMsg(status, 0x03, 0x7F);
        }
    }
};

DJCi200mk2._scratchEnable = function(deck) {
    var alpha = 1.0/8;
    var beta = alpha/32;
    engine.scratchEnable(deck, 248, 33 + 1/3, alpha, beta);
};

DJCi200mk2._convertWheelRotation = function(value) {
    // When you rotate the jogwheel, the controller always sends either 0x1
    // (clockwise) or 0x7F (counter clockwise). 0x1 should map to 1, 0x7F
    // should map to -1 (IOW it's 7-bit signed).
    return value < 0x40 ? 1 : -1;
};

// The touch action on the jog wheel's top surface
DJCi200mk2.wheelTouch = function(channel, control, value, _status, _group) {
    var deck = channel;
    if (value > 0) {
        //  Touching the wheel.
        if (engine.getValue("[Channel" + deck + "]", "play") !== 1 || DJCi200mk2.scratchButtonState) {
            DJCi200mk2._scratchEnable(deck);
            DJCi200mk2.scratchAction[deck] = DJCi200mk2.kScratchActionScratch;
        } else {
            DJCi200mk2.scratchAction[deck] = DJCi200mk2.kScratchActionBend;
        }
    } else {
        // Released the wheel.
        engine.scratchDisable(deck);
        DJCi200mk2.scratchAction[deck] = DJCi200mk2.kScratchActionNone;
    }
};

// The touch action on the jog wheel's top surface while holding shift
DJCi200mk2.wheelTouchShift = function(channel, control, value, _status, _group) {
    var deck = channel - 3;
    // We always enable scratching regardless of button state.
    if (value > 0) {
        DJCi200mk2._scratchEnable(deck);
        DJCi200mk2.scratchAction[deck] = DJCi200mk2.kScratchActionSeek;
    } else {
        // Released the wheel.
        engine.scratchDisable(deck);
        DJCi200mk2.scratchAction[deck] = DJCi200mk2.kScratchActionNone;
    }
};

// Scratching on the jog wheel (rotating it while pressing the top surface)
DJCi200mk2.scratchWheel = function(channel, control, value, status, _group) {
    var deck;
    switch (status) {
    case 0xB1:
    case 0xB4:
        deck  = 1;
        break;
    case 0xB2:
    case 0xB5:
        deck  = 2;
        break;
    default:
        return;
    }
    var interval = DJCi200mk2._convertWheelRotation(value);
    var scratchAction = DJCi200mk2.scratchAction[deck];
    if (scratchAction === DJCi200mk2.kScratchActionScratch) {
        engine.scratchTick(deck, interval * DJCi200mk2.scratchScale);
    } else if (scratchAction === DJCi200mk2.kScratchActionSeek) {
        engine.scratchTick(deck,
            interval *  DJCi200mk2.scratchScale *
            DJCi200mk2.scratchShiftMultiplier);
    } else {
        engine.setValue(
            "[Channel" + deck + "]", "jog", interval * DJCi200mk2.bendScale);
    }
};

// Bending on the jog wheel (rotating using the edge)
DJCi200mk2.bendWheel = function(channel, control, value, _status, _group) {
    var interval = DJCi200mk2._convertWheelRotation(value);
    engine.setValue(
        "[Channel" + channel + "]", "jog", interval * DJCi200mk2.bendScale);
};

DJCi200mk2.shutdown = function() {
    midi.sendShortMsg(0xB0, 0x7F, 0x7E);
    midi.sendShortMsg(0x90, 0x04, 0x00);
};
