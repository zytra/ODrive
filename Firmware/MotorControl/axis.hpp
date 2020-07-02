#ifndef __AXIS_HPP
#define __AXIS_HPP

#ifndef __ODRIVE_MAIN_H
#error "This file should not be included directly. Include odrive_main.h instead."
#endif

class Axis {
public:
    enum Error_t {
        ERROR_NONE = 0x00,
        ERROR_INVALID_STATE = 0x01, //<! an invalid state was requested
        ERROR_DC_BUS_UNDER_VOLTAGE = 0x02,
        ERROR_DC_BUS_OVER_VOLTAGE = 0x04,
        ERROR_CURRENT_MEASUREMENT_TIMEOUT = 0x08,
        ERROR_BRAKE_RESISTOR_DISARMED = 0x10, //<! the brake resistor was unexpectedly disarmed
        ERROR_MOTOR_DISARMED = 0x20, //<! the motor was unexpectedly disarmed
        ERROR_MOTOR_FAILED = 0x40, // Go to motor.hpp for information, check odrvX.axisX.motor.error for error value 
        ERROR_SENSORLESS_ESTIMATOR_FAILED = 0x80,
        ERROR_ENCODER_FAILED = 0x100, // Go to encoder.hpp for information, check odrvX.axisX.encoder.error for error value
        ERROR_CONTROLLER_FAILED = 0x200,
        ERROR_POS_CTRL_DURING_SENSORLESS = 0x400, // DEPRECATED
        ERROR_WATCHDOG_TIMER_EXPIRED = 0x800,
        ERROR_MIN_ENDSTOP_PRESSED = 0x1000,
        ERROR_MAX_ENDSTOP_PRESSED = 0x2000,
        ERROR_ESTOP_REQUESTED = 0x4000,
        ERROR_HOMING_WITHOUT_ENDSTOP = 0x20000, // the min endstop was not enabled during homing
    };

    enum State_t {
        AXIS_STATE_UNDEFINED = 0,           //<! will fall through to idle
        AXIS_STATE_IDLE = 1,                //<! disable PWM and do nothing
        AXIS_STATE_STARTUP_SEQUENCE = 2, //<! the actual sequence is defined by the config.startup_... flags
        AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3,   //<! run all calibration procedures, then idle
        AXIS_STATE_MOTOR_CALIBRATION = 4,   //<! run motor calibration
        AXIS_STATE_SENSORLESS_CONTROL = 5,  //<! run sensorless control
        AXIS_STATE_ENCODER_INDEX_SEARCH = 6, //<! run encoder index search
        AXIS_STATE_ENCODER_OFFSET_CALIBRATION = 7, //<! run encoder offset calibration
        AXIS_STATE_CLOSED_LOOP_CONTROL = 8,  //<! run closed loop control
        AXIS_STATE_LOCKIN_SPIN = 9,       //<! run lockin spin
        AXIS_STATE_ENCODER_DIR_FIND = 10,
        AXIS_STATE_HOMING = 11,   //<! run axis homing function
    };

    struct LockinConfig_t {
        float current = 10.0f;           // [A]
        float ramp_time = 0.4f;          // [s]
        float ramp_distance = 1 * M_PI;  // [rad]
        float accel = 20.0f;     // [rad/s^2]
        float vel = 40.0f; // [rad/s]
        float finish_distance = 100.0f;  // [rad]
        bool finish_on_vel = false;
        bool finish_on_distance = false;
        bool finish_on_enc_idx = false;
    };

    static LockinConfig_t default_calibration();
    static LockinConfig_t default_sensorless();
    static LockinConfig_t default_lockin();

    struct Config_t {
        bool startup_motor_calibration = false;   //<! run motor calibration at startup, skip otherwise
        bool startup_encoder_index_search = false; //<! run encoder index search after startup, skip otherwise
                                                // this only has an effect if encoder.config.use_index is also true
        bool startup_encoder_offset_calibration = false; //<! run encoder offset calibration after startup, skip otherwise
        bool startup_closed_loop_control = false; //<! enable closed loop control after calibration/startup
        bool startup_sensorless_control = false; //<! enable sensorless control after calibration/startup
        bool startup_homing = false; //<! enable homing after calibration/startup

        bool enable_step_dir = false; //<! enable step/dir input after calibration
                                    //   For M0 this has no effect if enable_uart is true
        bool step_dir_always_on = false; //<! Keep step/dir enabled while the motor is disabled.
                                         //<! This is ignored if enable_step_dir is false.
                                         //<! This setting only takes effect on a state transition
                                         //<! into idle or out of closed loop control.

        float counts_per_step = 2.0f;

        float watchdog_timeout = 0.0f; // [s]
        bool enable_watchdog = false;

        // Defaults loaded from hw_config in load_configuration in main.cpp
        uint16_t step_gpio_pin = 0;
        uint16_t dir_gpio_pin = 0;

        LockinConfig_t calibration_lockin = default_calibration();
        LockinConfig_t sensorless_ramp = default_sensorless();
        LockinConfig_t lockin;
        uint32_t can_node_id = 0; // Both axes will have the same id to start
        bool can_node_id_extended = false;
        uint32_t can_heartbeat_rate_ms = 100;
    };

    struct Homing_t {
        bool is_homed = false;
    };

    enum thread_signals {
        M_SIGNAL_PH_CURRENT_MEAS = 1u << 0
    };

    enum LockinState_t {
        LOCKIN_STATE_INACTIVE,
        LOCKIN_STATE_RAMP,
        LOCKIN_STATE_ACCELERATE,
        LOCKIN_STATE_CONST_VEL,
    };

    Axis(int axis_num,
            const AxisHardwareConfig_t& hw_config,
            Config_t& config,
            Encoder& encoder,
            SensorlessEstimator& sensorless_estimator,
            Controller& controller,
            Motor& motor,
            TrapezoidalTrajectory& trap,
            Endstop& min_endstop,
            Endstop& max_endstop);

    void setup();
    void start_thread();
    void signal_current_meas();
    bool wait_for_current_meas();

    void step_cb();
    void set_step_dir_active(bool enable);
    void decode_step_dir_pins();

    static void load_default_step_dir_pin_config(
        const AxisHardwareConfig_t& hw_config, Config_t* config);
    static void load_default_can_id(const int& id, Config_t& config);

    bool check_DRV_fault();
    bool check_PSU_brownout();
    bool do_checks();
    bool do_updates();

    void watchdog_feed();
    bool watchdog_check();

    void clear_errors() {
        motor_.error_ = Motor::ERROR_NONE;
        controller_.error_ = Controller::ERROR_NONE;
        sensorless_estimator_.error_ = SensorlessEstimator::ERROR_NONE;
        encoder_.error_ = Encoder::ERROR_NONE;
        encoder_.spi_error_rate_ = 0.0f;

        error_ = Axis::ERROR_NONE;
    }

    // True if there are no errors
    bool inline check_for_errors() {
        return error_ == ERROR_NONE;
    }

    // @brief Runs the specified update handler at the frequency of the current measurements.
    //
    // The loop runs until one of the following conditions:
    //  - update_handler returns false
    //  - the current measurement times out
    //  - the health checks fail (brownout, driver fault line)
    //  - update_handler doesn't update the modulation timings in time
    //    This criterion is ignored if current_state is AXIS_STATE_IDLE
    //
    // If update_handler is going to update the motor timings, you must call motor.arm()
    // shortly before this function.
    //
    // If the function returns, it is guaranteed that error is non-zero, except if the cause
    // for the exit was a negative return value of update_handler or an external
    // state change request (requested_state != AXIS_STATE_DONT_CARE).
    // Under all exit conditions the motor is disarmed and the brake current set to zero.
    // Furthermore, if the update_handler does not set the phase voltages in time, they will
    // go to zero.
    //
    // @tparam T Must be a callable type that takes no arguments and returns a bool
    template<typename T>
    void run_control_loop(const T& update_handler) {
        while (requested_state_ == AXIS_STATE_UNDEFINED) {
            // look for errors at axis level and also all subcomponents
            bool checks_ok = do_checks();
            // Update all estimators
            // Note: updates run even if checks fail
            bool updates_ok = do_updates(); 

            // make sure the watchdog is being fed. 
            bool watchdog_ok = watchdog_check();
            
            if (!checks_ok || !updates_ok || !watchdog_ok) {
                // It's not useful to quit idle since that is the safe action
                // Also leaving idle would rearm the motors
                if (current_state_ != AXIS_STATE_IDLE)
                    break;
            }

            // Run main loop function, defer quitting for after wait
            // TODO: change arming logic to arm after waiting
            bool main_continue = update_handler();

            // Check we meet deadlines after queueing
            ++loop_counter_;

            // Wait until the current measurement interrupt fires
            if (!wait_for_current_meas()) {
                // maybe the interrupt handler is dead, let's be
                // safe and float the phases
                safety_critical_disarm_motor_pwm(motor_);
                update_brake_current();
                error_ |= ERROR_CURRENT_MEASUREMENT_TIMEOUT;
                break;
            }

            if (!main_continue)
                break;
        }
    }

    bool run_lockin_spin(const LockinConfig_t &lockin_config);
    bool run_sensorless_control_loop();
    bool run_closed_loop_control_loop();
    bool run_homing();
    bool run_idle_loop();

    constexpr uint32_t get_watchdog_reset() {
        return static_cast<uint32_t>(std::clamp<float>(config_.watchdog_timeout, 0, UINT32_MAX / (current_meas_hz + 1)) * current_meas_hz);
    }

    void run_state_machine_loop();

    int axis_num_;
    const AxisHardwareConfig_t& hw_config_;
    Config_t& config_;

    Encoder& encoder_;
    SensorlessEstimator& sensorless_estimator_;
    Controller& controller_;
    Motor& motor_;
    TrapezoidalTrajectory& trap_;
    Endstop& min_endstop_;
    Endstop& max_endstop_;

    osThreadId thread_id_;
    const uint32_t stack_size_ = 2048; // Bytes
    volatile bool thread_id_valid_ = false;

    // variables exposed on protocol
    Error_t error_ = ERROR_NONE;
    bool step_dir_active_ = false; // auto enabled after calibration, based on config.enable_step_dir

    // updated from config in constructor, and on protocol hook
    GPIO_TypeDef* step_port_;
    uint16_t step_pin_;
    GPIO_TypeDef* dir_port_;
    uint16_t dir_pin_;

    State_t requested_state_ = AXIS_STATE_STARTUP_SEQUENCE;
    std::array<State_t, 10> task_chain_ = { AXIS_STATE_UNDEFINED };
    State_t& current_state_ = task_chain_.front();
    uint32_t loop_counter_ = 0;
    LockinState_t lockin_state_ = LOCKIN_STATE_INACTIVE;
    Homing_t homing_;
    uint32_t last_heartbeat_ = 0;

    // watchdog
    uint32_t watchdog_current_value_= 0;

    // Communication protocol definitions
    auto make_protocol_definitions() {
        return make_protocol_member_list(
            make_protocol_property("error", &error_),
            make_protocol_ro_property("step_dir_active", &step_dir_active_),
            make_protocol_ro_property("current_state", &current_state_),
            make_protocol_property("requested_state", &requested_state_),
            make_protocol_ro_property("loop_counter", &loop_counter_),
            make_protocol_ro_property("lockin_state", &lockin_state_),
            make_protocol_property("is_homed", &homing_.is_homed),
            make_protocol_object("config",
                make_protocol_property("startup_motor_calibration", &config_.startup_motor_calibration),
                make_protocol_property("startup_encoder_index_search", &config_.startup_encoder_index_search),
                make_protocol_property("startup_encoder_offset_calibration", &config_.startup_encoder_offset_calibration),
                make_protocol_property("startup_closed_loop_control", &config_.startup_closed_loop_control),
                make_protocol_property("startup_sensorless_control", &config_.startup_sensorless_control),
                make_protocol_property("startup_homing", &config_.startup_homing),
                make_protocol_property("enable_step_dir", &config_.enable_step_dir),
                make_protocol_property("step_dir_always_on", &config_.step_dir_always_on),
                make_protocol_property("counts_per_step", &config_.counts_per_step),
                make_protocol_property("watchdog_timeout", &config_.watchdog_timeout),
                make_protocol_property("enable_watchdog", &config_.enable_watchdog),
                make_protocol_property("step_gpio_pin", &config_.step_gpio_pin,
                                    [](void* ctx) { static_cast<Axis*>(ctx)->decode_step_dir_pins(); }, this),
                make_protocol_property("dir_gpio_pin", &config_.dir_gpio_pin,
                                    [](void* ctx) { static_cast<Axis*>(ctx)->decode_step_dir_pins(); }, this),
                make_protocol_object("calibration_lockin",
                    make_protocol_property("current", &config_.calibration_lockin.current),
                    make_protocol_property("ramp_time", &config_.calibration_lockin.ramp_time),
                    make_protocol_property("ramp_distance", &config_.calibration_lockin.ramp_distance),
                    make_protocol_property("accel", &config_.calibration_lockin.accel),
                    make_protocol_property("vel", &config_.calibration_lockin.vel)),
                make_protocol_object("sensorless_ramp",
                    make_protocol_property("current", &config_.sensorless_ramp.current),
                    make_protocol_property("ramp_time", &config_.sensorless_ramp.ramp_time),
                    make_protocol_property("ramp_distance", &config_.sensorless_ramp.ramp_distance),
                    make_protocol_property("accel", &config_.sensorless_ramp.accel),
                    make_protocol_property("vel", &config_.sensorless_ramp.vel),
                    make_protocol_property("finish_distance", &config_.sensorless_ramp.finish_distance),
                    make_protocol_property("finish_on_vel", &config_.sensorless_ramp.finish_on_vel),
                    make_protocol_property("finish_on_distance", &config_.sensorless_ramp.finish_on_distance),
                    make_protocol_property("finish_on_enc_idx", &config_.sensorless_ramp.finish_on_enc_idx)),
                make_protocol_object("general_lockin",
                    make_protocol_property("current", &config_.lockin.current),
                    make_protocol_property("ramp_time", &config_.lockin.ramp_time),
                    make_protocol_property("ramp_distance", &config_.lockin.ramp_distance),
                    make_protocol_property("accel", &config_.lockin.accel),
                    make_protocol_property("vel", &config_.lockin.vel),
                    make_protocol_property("finish_distance", &config_.lockin.finish_distance),
                    make_protocol_property("finish_on_vel", &config_.lockin.finish_on_vel),
                    make_protocol_property("finish_on_distance", &config_.lockin.finish_on_distance),
                    make_protocol_property("finish_on_enc_idx", &config_.lockin.finish_on_enc_idx)),
                make_protocol_property("can_node_id", &config_.can_node_id),
                make_protocol_property("can_node_id_extended", &config_.can_node_id_extended),
                make_protocol_property("can_heartbeat_rate_ms", &config_.can_heartbeat_rate_ms)),
            make_protocol_object("motor", motor_.make_protocol_definitions()),
            make_protocol_object("controller", controller_.make_protocol_definitions()),
            make_protocol_object("encoder", encoder_.make_protocol_definitions()),
            make_protocol_object("sensorless_estimator", sensorless_estimator_.make_protocol_definitions()),
            make_protocol_object("trap_traj", trap_.make_protocol_definitions()),
            make_protocol_object("min_endstop", min_endstop_.make_protocol_definitions()),
            make_protocol_object("max_endstop", max_endstop_.make_protocol_definitions()),
            make_protocol_function("watchdog_feed", *this, &Axis::watchdog_feed),
            make_protocol_function("clear_errors", *this, &Axis::clear_errors)
        );
    }
};


DEFINE_ENUM_FLAG_OPERATORS(Axis::Error_t)

#endif /* __AXIS_HPP */
