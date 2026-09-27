`PidCtrl.h` / `PidCtrl.cpp` are copied unchanged from the Arduino firmware
(`coffee_ctrl_main`). They serve as the golden reference for `control::PidController`
in `test_pid_controller.cpp`. `Arduino.h` is a shim that lets the test control `millis()`.
