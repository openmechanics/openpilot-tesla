// board enforces
//   in-state
//      accel set/resume
//   out-state
//      cancel button
//      accel rising edge
//      brake rising edge
//      brake > 0mph

// these are set in the Honda safety hooks...this is the wrong place
const int gas_interceptor_threshold = 328;
int gas_interceptor_detected = 0;
int brake_prev = 0;
int gas_prev = 0;
int gas_interceptor_prev = 0;
int ego_speed = 0;
// TODO: auto-detect bosch hardware based on CAN messages?
bool bosch_hardware = false;

static void honda_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  // sample speed
  if ((to_push->RIR>>21) == 0x158) {
    // first 2 bytes
    ego_speed = to_push->RDLR & 0xFFFF;
  }

  // state machine to enter and exit controls
  // 0x1A6 for the ILX, 0x296 for the Civic Touring
  if ((to_push->RIR>>21) == 0x1A6 || (to_push->RIR>>21) == 0x296) {
    int buttons = (to_push->RDLR & 0xE0) >> 5;
    if (buttons == 4 || buttons == 3) {
      controls_allowed = 1;
    } else if (buttons == 2) {
      controls_allowed = 0;
    }
  }

  // user brake signal is different for nidec vs bosch hardware
  // nidec hardware: 0x17C bit 53
  // bosch hardware: 0x1BE bit 4
  #define IS_USER_BRAKE_MSG(to_push) (!bosch_hardware ? to_push->RIR>>21 == 0x17C : to_push->RIR>>21 == 0x1BE)
  #define USER_BRAKE_VALUE(to_push)  (!bosch_hardware ? to_push->RDHR & 0x200000  : to_push->RDLR & 0x10)
  // exit controls on rising edge of brake press or on brake press when
  // speed > 0
  if (IS_USER_BRAKE_MSG(to_push)) {
    int brake = USER_BRAKE_VALUE(to_push);
    if (brake && (!(brake_prev) || ego_speed)) {
      controls_allowed = 0;
    }
    brake_prev = brake;
  }

  // exit controls on rising edge of gas press if interceptor (0x201 w/ len = 6)
  // length check because bosch hardware also uses this id (0x201 w/ len = 8)
  if ((to_push->RIR>>21) == 0x201 && (to_push->RDTR & 0xf) == 6) {
    gas_interceptor_detected = 1;
    int gas_interceptor = ((to_push->RDLR & 0xFF) << 8) | ((to_push->RDLR & 0xFF00) >> 8);
    if ((gas_interceptor > gas_interceptor_threshold) &&
        (gas_interceptor_prev <= gas_interceptor_threshold)) {
      controls_allowed = 0;
    }
    gas_interceptor_prev = gas_interceptor;
  }

  // exit controls on rising edge of gas press if no interceptor
  if (!gas_interceptor_detected) {
    if ((to_push->RIR>>21) == 0x17C) {
      int gas = to_push->RDLR & 0xFF;
      if (gas && !(gas_prev)) {
        controls_allowed = 0;
      }
      gas_prev = gas;
    }
  }
}

// all commands: gas, brake and steering
// if controls_allowed and no pedals pressed
//     allow all commands up to limit
// else
//     block all commands that produce actuation

static int honda_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  // disallow actuator commands if gas or brake (with vehicle moving) are pressed
  // and the the latching controls_allowed flag is True
  int pedal_pressed = gas_prev || (gas_interceptor_prev > gas_interceptor_threshold) ||
                      (brake_prev && ego_speed);
  int current_controls_allowed = controls_allowed && !(pedal_pressed);

  // BRAKE: safety check
  if ((to_send->RIR>>21) == 0x1FA) {
    if (current_controls_allowed) {
      if ((to_send->RDLR & 0xFFFFFF3F) != to_send->RDLR) return 0;
    } else {
      if ((to_send->RDLR & 0xFFFF0000) != to_send->RDLR) return 0;
    }
  }

  // STEER: safety check
  if ((to_send->RIR>>21) == 0xE4 || (to_send->RIR>>21) == 0x194) {
    if (current_controls_allowed) {
      // all messages are fine here
    } else {
      if ((to_send->RDLR & 0xFFFF0000) != to_send->RDLR) return 0;
    }
  }

  // GAS: safety check
  if ((to_send->RIR>>21) == 0x200) {
    if (current_controls_allowed) {
      // all messages are fine here
    } else {
      if ((to_send->RDLR & 0xFFFF0000) != to_send->RDLR) return 0;
    }
  }

  // 1 allows the message through
  return true;
}

static int honda_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  // TODO: add safety if using LIN
  return true;
}

static void honda_init(int16_t param) {
  controls_allowed = 0;
  bosch_hardware = false;
}

static int honda_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  return -1;
}

static int honda_ign_hook() {
  return -1;
}

const safety_hooks honda_hooks = {
  .init = honda_init,
  .rx = honda_rx_hook,
  .tx = honda_tx_hook,
  .tx_lin = honda_tx_lin_hook,
  .ignition = honda_ign_hook,
  .fwd = honda_fwd_hook,
};

static void honda_bosch_init(int16_t param) {
  controls_allowed = 0;
  bosch_hardware = true;
}

static int honda_bosch_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  if (bus_num == 1 || bus_num == 2) {
    int addr = to_fwd->RIR>>21;
    return addr != 0xE4 && addr != 0x33D ? (uint8_t)(~bus_num & 0x3) : -1;
  }
  return -1;
}

const safety_hooks honda_bosch_hooks = {
  .init = honda_bosch_init,
  .rx = honda_rx_hook,
  .tx = honda_tx_hook,
  .tx_lin = honda_tx_lin_hook,
  .ignition = honda_ign_hook,
  .fwd = honda_bosch_fwd_hook,
};
