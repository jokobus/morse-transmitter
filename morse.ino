// Arduino: Nano Atmel mega328p
// LED: WS2812B (CJMCU-2812-8)
// Used: Timer1 CTC, prescaler 1024, Button interupt

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>    // LED
#include <EEPROM.h>               // State persistence (button press handling)
// Message
const char MESSAGE[] = "IN";      ///< change this to whatever message you want

// Pin setup
const uint8_t LED_PIN = 6;     ///< (Digital) Pin of LED stripe
const uint8_t WAVE_WIDTH = 5;  ///< set to 0 for normal blinking, >0 for lighthouse effect
const uint8_t LED_COUNT = 8;   ///< total number of LEDs in the strip

// Morse Code Timing Configuration (in milliseconds)
const uint16_t TIME_DOT = 500;          ///< Duration of a dot
const uint16_t TIME_DASH = 1500;        ///< Duration of a dash
const uint16_t TIME_SYMBOL_GAP = 500;   ///< Gap between dots/dashes within a character
const uint16_t TIME_LETTER_GAP = 1500;  ///< Gap between characters
const uint16_t TIME_WORD_GAP = 3500;    ///< Gap between words
const uint8_t BRIGHTNESS = 1;           ///< Brightness in % from 0 to 100%, currently 1% very dim

// Init an instance called led_strip of Adafluit_NeoPixel class
Adafruit_NeoPixel led_strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Timing in seconds, scaled to Timer1 ticks each ISR reload
const uint16_t TICKS_PER_SECOND = 15625;  ///< 16MHz / 1024 = 15625
const uint8_t MAX_STEPS = 100;            ///< Sufficient for most messages

// arrays to store message
uint16_t durations[MAX_STEPS];            ///< duration in milliseconds per step
uint32_t colors[MAX_STEPS];               ///< RGB color for each step (packed as 0xRRGGBB)
uint8_t ledIndex[MAX_STEPS];              ///< Index of center LED at each time step in [0;LED_COUNT] with 255 = all LEDs
uint8_t stepCount = 0;                    ///< Total number of steps in the sequence
volatile uint8_t currentStep = 0;         ///< Current step index being executed

/**
 * @brief Lookup table for Morse symbols.
 * 
 * Converts a single character into its Morse code representation
 * using dots ('.') and dashes ('-').
 * 
 * @param c The character to convert (case-insensitive handling done by caller).
 * @return const char* String representing the Morse code, or empty string if unknown.
 */
const char* morseCode(char c) {
  switch (c) {
    case 'A': return ".-";
    case 'B': return "-...";
    case 'C': return "-.-.";
    case 'D': return "-..";
    case 'E': return ".";
    case 'F': return "..-.";
    case 'G': return "--.";
    case 'H': return "....";
    case 'I': return "..";
    case 'J': return ".---";
    case 'K': return "-.-";
    case 'L': return ".-..";
    case 'M': return "--";
    case 'N': return "-.";
    case 'O': return "---";
    case 'P': return ".--.";
    case 'Q': return "--.-";
    case 'R': return ".-.";
    case 'S': return "...";
    case 'T': return "-";
    case 'U': return "..-";
    case 'V': return "...-";
    case 'W': return ".--";
    case 'X': return "-..-";
    case 'Y': return "-.--";
    case 'Z': return "--..";
    case '0': return "-----";
    case '1': return ".----";
    case '2': return "..---";
    case '3': return "...--";
    case '4': return "....-";
    case '5': return ".....";
    case '6': return "-....";
    case '7': return "--...";
    case '8': return "---..";
    case '9': return "----.";
    default:  return "";
  }
}

/**
 * @brief Adds a single step to the Morse sequence.
 * 
 * Records the duration and color for a specific part of the sequence.
 * 
 * @param duration_ms Duration of the step in milliseconds.
 * @param color RGB color (use led_strip.Color() or 0 for off).
 * @param ledIdx LED index to light (255 = all LEDs).
 */
void addStep(uint16_t duration_ms, uint32_t color, uint8_t ledIdx = 255) {
  if (stepCount < MAX_STEPS) {
    durations[stepCount] = duration_ms;
    colors[stepCount] = color;
    ledIndex[stepCount] = ledIdx;
    stepCount++;
  }
}

/**
 * @brief Constructs the full Morse code sequence for the global MESSAGE.
 * 
 * Converts each character to Morse code and creates a lighthouse wave effect
 * where the light sweeps across all LEDs for each dot/dash.
 * Adds appropriate gaps between symbols, letters, and words.
 * Includes a final wave effect at the end before looping.
 */
void buildSequence() {
  stepCount = 0;
  const uint32_t WHITE = led_strip.Color(255, 255, 255);
  const uint32_t GREEN = led_strip.Color(0, 255, 0);  // not used right now
  const uint32_t RED = led_strip.Color(255, 0, 0);    // not used right now
  const uint32_t OFF = 0;
  
  // loop through the message
  for (const char* p = MESSAGE; *p; ++p) {
    char c = toupper(*p);
    if (c == ' ') {
      addStep(TIME_WORD_GAP, OFF); // space between words
      continue;
    }
    const char* code = morseCode(c);
    if (!*code) continue;  // skip unknown characters

    // go through each dot or dash
    for (const char* s = code; *s; ++s) {
      uint16_t onDur = (*s == '.') ? TIME_DOT : TIME_DASH;
      uint16_t perLedDur = onDur / LED_COUNT;
      
      // lighthouse wave - light moves across the strip
      for (uint8_t i = 0; i < LED_COUNT; i++) {
        addStep(perLedDur, WHITE, i);
      }
      
      if (*(s + 1)) {
        addStep(TIME_SYMBOL_GAP, OFF);  // gap between dots/dashes
      }
    }
    addStep(TIME_LETTER_GAP, OFF);  // gap between letters
  }
  // End of message
  addStep(TIME_WORD_GAP, OFF);
  
  // cool reverse wave at the end before looping
  for (uint8_t i = LED_COUNT; i > 0; i--) {
    addStep(20, WHITE, i);
  }
  addStep(TIME_WORD_GAP, OFF);
  
}

/**
 * @brief Timer1 Compare Match A Interrupt Service Routine.
 * 
 * Updates the LED state for the current step in the sequence.
 * Handles both full strip control and wave effects with multiple LEDs.
 * The sequence loops automatically when complete.
 * 
 * Note: LED updates can block for ~300 microseconds (WS2812B protocol).
 * This is acceptable given the slow Morse code timing (500ms+).
 */
ISR(TIMER1_COMPA_vect) {
  // Update LED state - either individual LED or all LEDs
  if (ledIndex[currentStep] == 255 || WAVE_WIDTH == 0) {
    // Light all LEDs (standard mode or wave disabled)
    led_strip.fill(colors[currentStep]);
  } else {
    // wave effect with multiple neighbouring LEDs
    led_strip.clear();
    if (colors[currentStep] != 0) {
      // Light up center LED and neighbours
      int8_t center = ledIndex[currentStep];
      int8_t halfWidth = WAVE_WIDTH / 2;
      
      for (int8_t offset = -halfWidth; offset <= halfWidth; offset++) {
        int8_t ledPos = center + offset;
        // Only light up LEDs within strip boundaries -> else unexpected behaviour with leds on at wrong "end" of strip (like index was taken modulo by number of leds in strip)
        if (ledPos >= 0 && ledPos < LED_COUNT) {
          led_strip.setPixelColor(ledPos, colors[currentStep]);
        }
      }
    }
  }
  led_strip.show();
  
  // Calculate how many timer ticks for current step duration
  uint32_t ticks = ((uint32_t)durations[currentStep] * TICKS_PER_SECOND) / 1000UL;
  if (ticks < 100) ticks = 100;  // minimum value
  
  OCR1A = (uint16_t)(ticks - 1);
  
  currentStep++;
  
  // loop back to start
  if (currentStep >= stepCount) {
    currentStep = 0;
  }
}

/**
 * @brief Displays startup sequence: Red -> Green -> Off.
 */
void showStartupSequence() {
  led_strip.fill(led_strip.Color(255, 0, 0)); // Red
  led_strip.show();
  delay(500);
  led_strip.fill(led_strip.Color(0, 255, 0)); // Green
  led_strip.show();
  delay(500);
  led_strip.fill(0); // Off
  led_strip.show();
  delay(500);
}

/**
 * @brief Displays shutdown sequence: Green -> Red -> Off.
 */
void showShutdownSequence() {
  led_strip.fill(led_strip.Color(0, 255, 0)); // Green
  led_strip.show();
  delay(500);
  led_strip.fill(led_strip.Color(255, 0, 0)); // Red
  led_strip.show();
  delay(500);
  led_strip.fill(0); // Off
  led_strip.show();
}

/**
 * @brief Arduino setup function.
 * 
 * Toggles between playing and stopped states using EEPROM.
 * Reset button press during play: restarts from beginning
 * Reset button press when stopped: starts playing
 */
void setup() {
  led_strip.begin();
  
  // Calculate and set brightness
  uint8_t brightness = (255 * BRIGHTNESS) / 100;
  led_strip.setBrightness(brightness);   

  // Using EEPROM to remember state between resets
  // Pressing reset button toggles on / off
  
  // Read state (0 = Stopped, 1 = Playing)
  byte state = EEPROM.read(0);
  state = !state;  // toggle
  
  // Actually EEPROM has ~100k write wear -> Only write if changed and thus really necessary to write
  if (EEPROM.read(0) != state) {
    EEPROM.write(0, state);
  }
  
  if (state) {
    // Start messaging
    showStartupSequence();
    
    currentStep = 0;
    buildSequence();

    cli();  // disable interrupts
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1B |= (1 << WGM12);              // CTC mode
    TCCR1B |= (1 << CS12) | (1 << CS10); // prescaler 1024
    OCR1A = 1562;                        // ~100ms
    TIMSK1 |= (1 << OCIE1A);             // enable interrupt
    sei();  // enable interrupts
  } else {
    // Stop messaging
    showShutdownSequence();
    
    // Disable Timer1 interrupt
    TIMSK1 &= ~(1 << OCIE1A);
    TCCR1B = 0;  // Stop timer
  }
}

/**
 * @brief Main application loop.
 * 
 * Empty - all Morse code logic is interrupt-driven via Timer1.
 */
void loop() {
  // main loop free for other tasks
}