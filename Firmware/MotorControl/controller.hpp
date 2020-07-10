#ifndef __CONTROLLER_HPP
#define __CONTROLLER_HPP

#ifndef __ODRIVE_MAIN_H
#error "This file should not be included directly. Include odrive_main.h instead."
#endif

class Controller {
public:
    enum Error_t {
        ERROR_NONE                 = 0,
        ERROR_OVERSPEED            = 0x01,
        ERROR_INVALID_INPUT_MODE   = 0x02,
        ERROR_UNSTABLE_GAIN        = 0x04,
        ERROR_INVALID_MIRROR_AXIS  = 0x08,
        ERROR_INVALID_LOAD_ENCODER = 0x10,
        ERROR_INVALID_ESTIMATE     = 0x20,
    };

    // Note: these should be sorted from lowest level of control to
    // highest level of control, to allow "<" style comparisons.
    enum ControlMode_t{
        CTRL_MODE_VOLTAGE_CONTROL = 0,
        CTRL_MODE_CURRENT_CONTROL = 1,
        CTRL_MODE_VELOCITY_CONTROL = 2,
        CTRL_MODE_POSITION_CONTROL = 3
    };

    enum InputMode_t{
        INPUT_MODE_INACTIVE,
        INPUT_MODE_PASSTHROUGH,
        INPUT_MODE_VEL_RAMP,
        INPUT_MODE_POS_FILTER,
        INPUT_MODE_MIX_CHANNELS,
        INPUT_MODE_TRAP_TRAJ,
        INPUT_MODE_CURRENT_RAMP,
        INPUT_MODE_MIRROR,
    };

    typedef struct {
        uint32_t index = 0;
        float cogging_map[3600];
        bool pre_calibrated = false;
        bool calib_anticogging = false;
        float calib_pos_threshold = 1.0f;
        float calib_vel_threshold = 1.0f;
        float cogging_ratio = 1.0f;
        bool enable = true;
    } Anticogging_t;

    struct Config_t {
        ControlMode_t control_mode = CTRL_MODE_POSITION_CONTROL;  //see: ControlMode_t
        InputMode_t input_mode = INPUT_MODE_PASSTHROUGH;  //see: InputMode_t
        float pos_gain = 20.0f;                         // [(counts/s) / counts]
        float vel_gain = 5.0f / 10000.0f;               // [A/(counts/s)]
        // float vel_gain = 5.0f / 200.0f,              // [A/(rad/s)] <sensorless example>
        float vel_integrator_gain = 10.0f / 10000.0f;   // [A/(counts/s * s)]
        float vel_limit = 20000.0f;                     // [counts/s] Infinity to disable.
        float vel_limit_tolerance = 1.2f;               // ratio to vel_lim. Infinity to disable.
        float vel_ramp_rate = 10000.0f;                 // [(counts/s) / s]
        float current_ramp_rate = 1.0f;                 // A / sec
        bool setpoints_in_cpr = false;
        float inertia = 0.0f;                           // [A/(count/s^2)]
        float input_filter_bandwidth = 2.0f;            // [1/s]
        float homing_speed = 2000.0f;                   // [counts/s]
        Anticogging_t anticogging;
        float gain_scheduling_width = 10.0f;
        bool enable_gain_scheduling = false;
        bool enable_vel_limit = true;
        bool enable_overspeed_error = true;
        bool enable_current_vel_limit = true;           // enable velocity limit in current control mode (requires a valid velocity estimator)
        uint8_t axis_to_mirror = -1;
        float mirror_ratio = 1.0f;
        uint8_t load_encoder_axis = -1;                 // default depends on Axis number and is set in load_configuration()
    };

    explicit Controller(Config_t& config);
    void reset();
    void set_error(Error_t error);

    void input_pos_updated();
    bool select_encoder(size_t encoder_num);

    // Trajectory-Planned control
    void move_to_pos(float goal_point);
    void move_incremental(float displacement, bool from_goal_point);
    
    // TODO: make this more similar to other calibration loops
    void start_anticogging_calibration();
    bool anticogging_calibration(float pos_estimate, float vel_estimate);

    void update_filter_gains();
    bool update(float* current_setpoint);

    Config_t& config_;
    Axis* axis_ = nullptr; // set by Axis constructor

    Error_t error_ = ERROR_NONE;

    float* pos_estimate_src_ = nullptr;
    bool* pos_estimate_valid_src_ = nullptr;
    float* vel_estimate_src_ = nullptr;
    bool* vel_estimate_valid_src_ = nullptr;
    int32_t* pos_wrap_src_ = nullptr; // enables circular position setpoints if not null. The value pointed to is the maximum position value.

    float pos_setpoint_ = 0.0f;
    float vel_setpoint_ = 0.0f;
    // float vel_setpoint = 800.0f; <sensorless example>
    float vel_integrator_current_ = 0.0f;  // [A]
    float current_setpoint_ = 0.0f;        // [A]

    float input_pos_ = 0.0f;
    float input_vel_ = 0.0f;
    float input_current_ = 0.0f;
    float input_filter_kp_ = 0.0f;
    float input_filter_ki_ = 0.0f;

    bool input_pos_updated_ = false;
    
    bool trajectory_done_ = true;

    bool anticogging_valid_ = false;

    // iq controller
    float iq_controller_ = 0.0f;

    // Communication protocol definitions
    auto make_protocol_definitions() {
        return make_protocol_member_list(
            make_protocol_property("error", &error_),
            make_protocol_property("input_pos", &input_pos_,
                [](void* ctx) { static_cast<Controller*>(ctx)->input_pos_updated(); }, this),
            make_protocol_property("input_vel", &input_vel_),
            make_protocol_property("input_current", &input_current_),
            make_protocol_ro_property("pos_setpoint", &pos_setpoint_),
            make_protocol_ro_property("vel_setpoint", &vel_setpoint_),
            make_protocol_ro_property("current_setpoint", &current_setpoint_),
            make_protocol_ro_property("trajectory_done", &trajectory_done_),
            make_protocol_property("vel_integrator_current", &vel_integrator_current_),
            make_protocol_property("anticogging_valid", &anticogging_valid_),
            make_protocol_property("gain_scheduling_width", &config_.gain_scheduling_width),
            make_protocol_property("iq_controller", &iq_controller_),
            make_protocol_object("config",
                make_protocol_property("enable_vel_limit", &config_.enable_vel_limit),
                make_protocol_property("enable_current_mode_vel_limit", &config_.enable_current_vel_limit),
                make_protocol_property("enable_gain_scheduling", &config_.enable_gain_scheduling),
                make_protocol_property("enable_overspeed_error", &config_.enable_overspeed_error),
                make_protocol_property("control_mode", &config_.control_mode),
                make_protocol_property("input_mode", &config_.input_mode),
                make_protocol_property("pos_gain", &config_.pos_gain),
                make_protocol_property("vel_gain", &config_.vel_gain),
                make_protocol_property("vel_integrator_gain", &config_.vel_integrator_gain),
                make_protocol_property("vel_limit", &config_.vel_limit),
                make_protocol_property("vel_limit_tolerance", &config_.vel_limit_tolerance),
                make_protocol_property("vel_ramp_rate", &config_.vel_ramp_rate),
                make_protocol_property("current_ramp_rate", &config_.current_ramp_rate),
                make_protocol_property("homing_speed", &config_.homing_speed),
                make_protocol_property("inertia", &config_.inertia),
                make_protocol_property("axis_to_mirror", &config_.axis_to_mirror),
                make_protocol_property("mirror_ratio", &config_.mirror_ratio),
                make_protocol_property("load_encoder_axis", &config_.load_encoder_axis),
                make_protocol_property("input_filter_bandwidth", &config_.input_filter_bandwidth,
                                    [](void* ctx) { static_cast<Controller*>(ctx)->update_filter_gains(); }, this),
                make_protocol_object("anticogging",
                    make_protocol_ro_property("index", &config_.anticogging.index),
                    make_protocol_property("pre_calibrated", &config_.anticogging.pre_calibrated),
                    make_protocol_ro_property("calib_anticogging", &config_.anticogging.calib_anticogging),
                    make_protocol_property("calib_pos_threshold", &config_.anticogging.calib_pos_threshold),
                    make_protocol_property("calib_vel_threshold", &config_.anticogging.calib_vel_threshold),
                    make_protocol_ro_property("cogging_ratio", &config_.anticogging.cogging_ratio),
                    make_protocol_property("anticogging_enabled", &config_.anticogging.enable))),
            make_protocol_function("move_incremental", *this, &Controller::move_incremental, "displacement", "from_goal_point"),
            make_protocol_function("start_anticogging_calibration", *this, &Controller::start_anticogging_calibration)
        );
    }
};

DEFINE_ENUM_FLAG_OPERATORS(Controller::Error_t)

#endif // __CONTROLLER_HPP
