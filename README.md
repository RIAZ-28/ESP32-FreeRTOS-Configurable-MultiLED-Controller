# ESP32-FreeRTOS-Configurable-MultiLED-Controller

## What it does

This project is a redesigned version of the original ESP32 FreeRTOS multi-LED
controller.

The first implementation used a separate LED task with a long delay for each
LED. Although the LEDs followed their assigned timing correctly, control
responses felt slow because each task could remain blocked inside its delay.

This version solves that problem by using a **single reusable LED task function**
with per-LED configuration and a much shorter task cycle.

Each LED is assigned a configuration containing:

* GPIO pin
* Blink duration

The same `led_task()` function is then reused for all three LEDs.

A UART interface accepts:

* `'1'` → all LEDs ON
* `'0'` → all LEDs OFF
* `'b'` → normal blinking

A push button can pause the LED operation.

## Problem identified in Version 1

The original implementation created a separate task function for every LED and
used the LED's blink period as the task delay.

For example:

```c
vTaskDelay(1500 / portTICK_PERIOD_MS);
```

This meant that a task controlling a slow LED could spend up to 1.5 seconds
waiting before checking the latest system state again.

The LED timing itself was correct, but **control responsiveness was limited by the
task delay**.

The problem was therefore not the blinking logic itself. It was the task
architecture and scheduling granularity.

## How Version 2 solves it

Instead of making the task delay equal to the LED's blink duration, this version
uses a common **50 ms task cycle**.

Each LED keeps track of its own elapsed time:

```c
elapsed += 50;
```

When the configured duration is reached, the LED toggles:

```c
if (elapsed >= cfg->duration)
{
    GPIO_OUT_REG ^= (1 << cfg->PIN);
    elapsed = 0;
}
```

This separates two concepts:

**Task execution interval**
→ 50 ms

**LED blink duration**
→ configurable for each LED

As a result, the task checks the latest `mode` and `paused` state much more
frequently while still allowing each LED to blink at its own configured rate.

## Reusable task architecture

Rather than maintaining three separate functions such as:

```text
led1_task()
led2_task()
led3_task()
```

the project defines one reusable task:

```text
led_task()
```

Each LED receives its own configuration:

| LED   | GPIO   | Duration |
| ----- | ------ | -------- |
| LED 1 | GPIO 2 | 500 ms   |
| LED 2 | GPIO 4 | 1000 ms  |
| LED 3 | GPIO 5 | 1500 ms  |

This makes the implementation easier to extend without duplicating the LED
control logic.

Adding another LED only requires another configuration structure and another
instance of the same task.

## Mutex-based GPIO synchronization

All LED tasks access the same `GPIO_OUT_REG`.

Because GPIO operations use read-modify-write operations, simultaneous access
from multiple tasks can cause one task's update to overwrite another task's
change.

A FreeRTOS mutex protects access to the shared GPIO register:

```c
xSemaphoreTake(gpio_mutex, portMAX_DELAY);

GPIO_OUT_REG ^= (1 << cfg->PIN);

xSemaphoreGive(gpio_mutex);
```

This ensures that GPIO register modifications are serialized between tasks.

## How it works

* UART task receives control commands
* Button task monitors the pause input
* Three instances of the same `led_task()` function control the LEDs
* Each task receives a different `led_config_t`
* Each task runs at a 50 ms scheduling interval
* An independent elapsed-time counter determines when each LED toggles
* GPIO register access is protected using a FreeRTOS mutex
* Direct GPIO register access is used instead of high-level GPIO APIs

## Hardware / Setup

**Components**

* ESP32 30 pin CP2102 development board
* 3x LED + 220Ω resistors
* 1x push button
* Breadboard
* jumper wires

**Pin Mapping**

| Component | ESP32 Pin |
| --------- | --------- |
| LED 1     | GPIO 2    |
| LED 2     | GPIO 4    |
| LED 3     | GPIO 5    |
| Button    | GPIO 19   |

**Wiring Notes**

* Button configured with external pull-up; reads LOW when pressed
* LEDs wired common cathode to GND through 220Ω resistor from each GPIO pin

**Software**

* ESP-IDF
* FreeRTOS (bundled with ESP-IDF)

**Build & Flash**

```bash
idf.py set-target esp32
idf.py build
idf.py -p port7 flash monitor
```

## What changed from Version 1

| Aspect               | Version 1                         | Version 2                     |
| -------------------- | --------------------------------- | ----------------------------- |
| LED task structure   | Separate function for each LED    | One reusable task             |
| Task delay           | LED-specific: 500/1000/1500 ms    | Common 50 ms cycle            |
| Blink timing         | Controlled directly by task delay | Controlled using elapsed time |
| Configuration        | Hard-coded inside each task       | `led_config_t` structure      |
| Code duplication     | Higher                            | Reduced                       |
| Control polling      | Less frequent for slower LEDs     | More frequent                 |
| GPIO synchronization | Mutex                             | Mutex                         |

The redesign separates **task scheduling frequency** from **LED blink frequency**,
making the system more responsive while keeping independent LED timing.

## Lessons learned

This project demonstrated that real-time behavior is not only about getting the
correct output, but also about **how frequently a system checks and reacts to
changing state**.

The redesign also demonstrated the benefits of:

* Reusable FreeRTOS task functions
* Configuration-driven design
* Mutex-based resource protection
* Separating execution frequency from application timing
* Identifying and solving responsiveness issues through architectural changes

## Next steps

* Replace polling with FreeRTOS event groups or task notifications
* Add proper button debounce
* Implement per-LED UART commands
* Explore FreeRTOS software timers for LED timing
* Measure and compare command response latency between both implementations

## Demo

[Short clip: three LEDs blinking at different configured durations, UART commands
'1'/'0'/'b', and button pause behavior]
