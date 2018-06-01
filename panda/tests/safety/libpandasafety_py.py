import os
import subprocess

from cffi import FFI

can_dir = os.path.dirname(os.path.abspath(__file__))
libpandasafety_fn = os.path.join(can_dir, "libpandasafety.so")
subprocess.check_call(["make"], cwd=can_dir)

ffi = FFI()
ffi.cdef("""
typedef struct
{
  uint32_t TIR;  /*!< CAN TX mailbox identifier register */
  uint32_t TDTR; /*!< CAN mailbox data length control and time stamp register */
  uint32_t TDLR; /*!< CAN mailbox data low register */
  uint32_t TDHR; /*!< CAN mailbox data high register */
} CAN_TxMailBox_TypeDef;

typedef struct
{
  uint32_t RIR;  /*!< CAN receive FIFO mailbox identifier register */
  uint32_t RDTR; /*!< CAN receive FIFO mailbox data length control and time stamp register */
  uint32_t RDLR; /*!< CAN receive FIFO mailbox data low register */
  uint32_t RDHR; /*!< CAN receive FIFO mailbox data high register */
} CAN_FIFOMailBox_TypeDef;

typedef struct
{
  uint32_t CNT;
} TIM_TypeDef;

void toyota_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int toyota_tx_hook(CAN_FIFOMailBox_TypeDef *to_send);
void toyota_init(int16_t param);
void set_controls_allowed(int c);
void reset_angle_control(void);
int get_controls_allowed(void);
void init_tests_toyota(void);
void set_timer(int t);
void set_torque_meas(int min, int max);
void set_rt_torque_last(int t);
void set_desired_torque_last(int t);
int get_torque_meas_min(void);
int get_torque_meas_max(void);

void init_tests_honda(void);
int get_ego_speed(void);
void honda_init(int16_t param);
void honda_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int honda_tx_hook(CAN_FIFOMailBox_TypeDef *to_send);
int get_brake_prev(void);
int get_gas_prev(void);

void toyota_ipas_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int toyota_ipas_tx_hook(CAN_FIFOMailBox_TypeDef *to_send);

""")

libpandasafety = ffi.dlopen(libpandasafety_fn)
