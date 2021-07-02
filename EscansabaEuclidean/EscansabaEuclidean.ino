
#include "euclid_core.h"
/* CONFIG */
static const milliseconds_t PULSE_WIDTH = 10; 
static const milliseconds_t TIME_UNTIL_INTERNAL_CLOCK_MODE = 4000; 
static const ClockMode CLOCK_MODE_ON_STARTUP = internal; //module starts up in internal clock mode... change this line to default the clock mode to external if needed

/* HARDWARE */
const int clockOutPin       = 1;
const int clockInPin        = 0;
const int barLengthKnobPin  = A1;
const int rotationKnobPin   = A3;
const int hitsKnobPin       = A2;

/* GLOBAL MUTABLE STATE */
milliseconds_t timeOfLastClockInChange;
euclid_t euclidRythmState;
ClockMode clockMode = CLOCK_MODE_ON_STARTUP; 
bool previousClockInputState = false;
InternalClock internalClock  = InternalClock {false, 0, 133};
bool shouldTrigger = false;

//ISR state
milliseconds_t timeOfLastPulseOut;


/* SHELL (global state and run loop) */

void setup()
{
  pinMode(clockOutPin, OUTPUT);
  pinMode(clockInPin, INPUT);
  digitalWrite(clockOutPin, LOW);
}

void loop()
{
  const milliseconds_t currentTime = millis();
  const bool isNewRisingClockEdge = setClockModeAndUpdateClock(currentTime);
  shouldTrigger = handleEuclidAlgorithmAndUpdateEuclidParams(isNewRisingClockEdge);
  
}

static inline void initTimer1(void)
{
 TCCR1 |= (1 << CTC1);  // clear timer on compare match
 
 TCCR1 |= (0 << CS13) | (1 << CS12) | (1 << CS11) | (0 << CS10); //timer hits at 1/32 of cpu speed
 //at 1mhz clock this is sample rate of 31250hz
 OCR1C = 0; // compare match value ... 0 means it matches every time it counts
 TIMSK |= (1 << OCIE1A); // enable compare match interrupt
}

ISR(TIMER1_COMPA_vect)
{
  const milliseconds_t currentTime = millis();
  milliseconds_t timeOfPulseOutToReturn = timeOfLastPulseOut;
  if (shouldTrigger) {
    digitalWrite(clockOutPin, HIGH);
    timeOfPulseOutToReturn = currentTime;
  } else if ((currentTime - timeOfLastPulseOut) > PULSE_WIDTH) {

    digitalWrite(clockOutPin, LOW);
  }
  timeOfLastPulseOut = timeOfPulseOutToReturn;
}

/* SHELL (IO) */

bool setClockModeAndUpdateClock(const milliseconds_t& currentTime){

  bool stateOfClockInPin = readClockInput();

  tuple <bool, milliseconds_t> ifChangedAndTime = didClockInputChange(stateOfClockInPin, previousClockInputState, currentTime, timeOfLastClockInChange);
  bool clockInputChanged = ifChangedAndTime.first;
  timeOfLastClockInChange = ifChangedAndTime.second;

  clockMode = whichClockModeShouldBeSet(clockInputChanged, clockMode, currentTime, timeOfLastClockInChange, TIME_UNTIL_INTERNAL_CLOCK_MODE);
  
  bool isNewRisingClockEdge;

  switch(clockMode){
    case internal: {
      bool previousInternalClockState = internalClock.clockSignalState;
      internalClock.tempo = readTempoInput();
      internalClock = updateInternalClock(currentTime, internalClock); 
      isNewRisingClockEdge = detectNewRisingClockEdge(internalClock.clockSignalState, previousInternalClockState);
      break;
    }
    case external: {
      isNewRisingClockEdge = detectNewRisingClockEdge(stateOfClockInPin, previousClockInputState); 
      break;
    }
  }
  previousClockInputState = stateOfClockInPin;

  return isNewRisingClockEdge;
}

bool handleEuclidAlgorithmAndUpdateEuclidParams(const bool& isNewRisingClockEdge){
  euclidRythmState = readEuclidParams( clockMode, isNewRisingClockEdge, euclidRythmState);
  const tuple<bool, euclid_t> shouldTriggerAndEuclidState = euclid_t::process(euclidRythmState);
  euclidRythmState = shouldTriggerAndEuclidState.second;
  bool shouldTrigger = shouldTriggerAndEuclidState.first;
  shouldTrigger &= isNewRisingClockEdge;
  return shouldTrigger;
}



bool readClockInput() {
  return !digitalRead(clockInPin);
}

euclid_t readEuclidParams(const ClockMode& mode, const bool& isNewRisingClockEdge, euclid_t params) {

  if (isNewRisingClockEdge) {
    params.barLengthInBeats = map(analogRead(barLengthKnobPin), 0, 1023, 1, 16);
    params.beats = map(analogRead(hitsKnobPin), 0, 1023, 0, 16);
    // params.counter = euclidRythmParameters.counter + 1;  
    
    if(mode==external){
      //should this map based on number of beats in a bar
      params.rotation = map(analogRead(rotationKnobPin), 0, 1023, 0, 16);            
    }
  }
  return params;
}

unsigned int readTempoInput(){
  // add debounce?
  const int inputFromRotationPin = analogRead(rotationKnobPin);

  return mapTempoInputToTempoInBpm(inputFromRotationPin);  
}
