#include "App/arm_control.h"


extern OD_Motor_Msg rv_motor_msg[10];

extern float magic_pos[3];
extern float magic_angle[3];
extern int magic_switch[2];

float arx_arm::joystick_projection(float joy_axis)

{
    float cmd = 0.0f;
    if(abs(joy_axis) >= JOYSTICK_DEADZONE)
    {
        cmd = (joy_axis - JOYSTICK_DEADZONE) / (1.0 - JOYSTICK_DEADZONE);
    }
    return cmd;
}

float arx_arm::ramp(float goal, float current, float ramp_k)
{
    float retval = 0.0f;
    float delta = 0.0f;
    delta = goal - current;
    if (delta > 0)
    {
        if (delta > ramp_k)
        {  
                current += ramp_k;
        }   
        else
        {
                current += delta;
        }
    }
    else
    {
        if (delta < -ramp_k)
        {
                current += -ramp_k;
        }
        else
        {
                current += delta;
        }
    }	
    retval = current;
    return retval;
}

arx_arm::arx_arm(ecat::EcatBase ecat_base,int CONTROL_MODE)
{

    // is_real = real_flg;
    control_mode=CONTROL_MODE;

    arx5_cmd.reset = false;
    arx5_cmd.x = 0.0;
    arx5_cmd.y = 0.0;
    arx5_cmd.z = 0.01; // if 0.0 is also ok but isaac sim joint 3 direction will be confused
    arx5_cmd.base_yaw = 0;
    arx5_cmd.gripper = 0;
    arx5_cmd.waist_roll = 0;
    arx5_cmd.waist_pitch = 0;
    arx5_cmd.waist_yaw = 0;
    arx5_cmd.mode = FORWARD;

    // Read robot model from urdf model
    if(control_mode == 0 ||  control_mode == 2 || control_mode ==4){
        model_path = ros::package::getPath("arm_control") + "/models/ultron_v1.1_aa.urdf"; 
    }else if(control_mode == 1 ||  control_mode == 3 || control_mode ==5) {
        model_path = ros::package::getPath("arm_control") + "/models/arx5h.urdf";
    }
    
    if (play.modifyLinkMass(model_path, model_path, 0.581)) { //默认单位kg 夹爪质量 0.381 0.581  0.0420
        std::cout << "Successfully modified the mass of link6." << std::endl;
    } else {
        std::cout << "Failed to modify the mass of link6." << std::endl;
    }    
    
    solve.solve_init(model_path);

    z_fd_filter = new LowPassFilter(200, 10);
    joint_vel_filter[0] = new LowPassFilter(200, 20);
    joint_vel_filter[1] = new LowPassFilter(200, 10);
    joint_vel_filter[2] = new LowPassFilter(200, 20);
    joint_vel_filter[3] = new LowPassFilter(200, 20);
    joint_vel_filter[4] = new LowPassFilter(200, 10);
    joint_vel_filter[5] = new LowPassFilter(200, 20);
    target_vel_filter[4] = new LowPassFilter(200, 30);
    
    if(control_mode == 0 || control_mode == 2 || control_mode==4){


    }else{
        
        }


}

void arx_arm::get_joint()
{
    current_pos[0] = rv_motor_msg[0].angle_actual_rad;
    current_pos[1] = rv_motor_msg[1].angle_actual_rad;
    current_pos[2] = rv_motor_msg[3].angle_actual_rad;
    current_pos[3] = rv_motor_msg[4].angle_actual_rad;
    current_pos[4] = rv_motor_msg[5].angle_actual_rad;
    current_pos[5] = rv_motor_msg[6].angle_actual_rad;
    current_pos[6] = rv_motor_msg[7].angle_actual_rad;    

    current_vel[0] = rv_motor_msg[0].speed_actual_rad;
    current_vel[1] = rv_motor_msg[1].speed_actual_rad;
    current_vel[2] = rv_motor_msg[3].speed_actual_rad;
    current_vel[3] = rv_motor_msg[4].speed_actual_rad;
    current_vel[4] = rv_motor_msg[5].speed_actual_rad;
    current_vel[5] = rv_motor_msg[6].speed_actual_rad;
    current_vel[6] = rv_motor_msg[7].speed_actual_rad;


    for(int i=0;i<6;i++)
    {
        // ROS_INFO("current_pos%i %f",i+1,current_pos[i]);
        if(current_pos[i]==0)
        ROS_ERROR("motor %d is not connected",i+1);
    }

    current_torque[0] = rv_motor_msg[0].current_actual_float;
    current_torque[1] = rv_motor_msg[1].current_actual_float;
    current_torque[2] = rv_motor_msg[3].current_actual_float;
    current_torque[3] = rv_motor_msg[4].current_actual_float;
    current_torque[4] = rv_motor_msg[5].current_actual_float;
    current_torque[5] = rv_motor_msg[6].current_actual_float;
    current_torque[6] = rv_motor_msg[7].current_actual_float;

    int set_max_torque = 9;
    
        for (float num : current_torque) {
            if (abs(num) > set_max_torque) {
                temp_current_normal++;
            }
        }

        for (float num : current_torque) {
            if(abs(num) > set_max_torque)
            {
                temp_condition = false;
            }
        }
        if(temp_condition)
        {
            temp_current_normal=0;
        }

        if(temp_current_normal>300)
        {
            current_normal = false;
        }

}

command arx_arm::get_cmd()
{
  
    if (is_recording)
    {
        play.update_record(current_pos);
        printf("is currently recording \n");
    }

    if (play.is_playing)
    {
        return arx5_cmd;
    }
    else
    {
        arm_torque_mode();
        arm_reset_mode();
        arm_get_pos();
        arm_teach_mode();                                      
    }
    return arx5_cmd;
}

void arx_arm::test_update(ecat::EcatBase ecat_base)
{
 
    CAN_Handlej.Send_moto_Cmd1(ecat_base, 1 ,0, 0, 0, 0, 0);
    CAN_Handlej.Send_moto_Cmd1(ecat_base, 2 ,0, 0, 0, 0, 0);
    CAN_Handlej.Send_moto_Cmd1(ecat_base, 4 ,0, 0, 0, 0, 0);
    CAN_Handlej.Send_moto_Cmd2(ecat_base, 5 ,0, 0, 0, 0, 0);
    CAN_Handlej.Send_moto_Cmd2(ecat_base, 6 ,0, 0, 0, 0, 0);
    CAN_Handlej.Send_moto_Cmd2(ecat_base, 7 ,0, 0, 0, 0, 0);
    CAN_Handlej.Send_moto_Cmd2(ecat_base, 8 ,0, 0, 0, 0, 0);ecat_base.EcatSyncMsg();CAN_Handlej.can0_ReceiveFrame(ecat_base);

}


void arx_arm::update_real(ecat::EcatBase ecat_base,command cmd)
{
    arx5_state state = NORMAL;
    solve.arm_calc1(arx5_cmd,current_pos,control_mode);
    if(is_starting)
    {    
        init_step(ecat_base);
    }
    else
    {
        if(control_mode == 0  ||  control_mode == 1 ||control_mode==4 || control_mode==5 )
        {
                if(play.is_playing)
                {
                    if(play.is_play_initing)
                    {
                        std::vector<std::string> init_read = play.getFilesList(ros::package::getPath("arm_control") + "/teach_record");
                        play.read_init(init_read);
                        play.read_end(init_read);
                        cmd_init();
                        play.init_step(target_pos,play.read_init_pos);
                    }else{
                        is_starting=0;
                        play.replay_step(target_pos,current_pos,play_file_list); 
                        if(target_pos[6]>200000)
                        {
                            play_gripper =1 ;
                        }else
                            play_gripper =0 ;
                    }

                }
                else
                {
                    solve.arm_calc2(arx5_cmd,current_pos,target_pos,control_mode,0);
                    // ROS_INFO("fk_pos is: x%f, y%f, z%f, r%f, p%f, y%f ", solve.End_Effector_Pose[0], solve.End_Effector_Pose[1], solve.End_Effector_Pose[2], solve.End_Effector_Pose[3], solve.End_Effector_Pose[4], solve.End_Effector_Pose[5]);

                }

                if(state == OUT_BOUNDARY) ROS_ERROR("waist command out of joint boundary!  please enter >>> R ");
                else if(state == OUT_RANGE) 
                {
                    ROS_ERROR("waist command out of arm range! please enter >>> R ");
                }
      
                if(teach2pos_returning ){
                    is_starting=1;
                }
                else{
                    is_starting=0;
                    motor_control(ecat_base);
                    
                }
        }

        if(control_mode == 2  ||  control_mode == 3)
        {
            joint_control(ecat_base);
            ROS_WARN("ROS_control>>>>>>>>>");
        }
            gripper_control(ecat_base);

    }
    return;
}

void arx_arm::motor_control(ecat::EcatBase ecat_base)
{
    if(current_normal)
    {
            if (is_teach_mode) {
                CAN_Handlej.Send_moto_Cmd1(ecat_base,1, 0, 0, 0, 0,  solve.joint_torque[0]); 
                CAN_Handlej.Send_moto_Cmd1(ecat_base,2, 0, 0, 0, 0,  solve.joint_torque[1]); 
                CAN_Handlej.Send_moto_Cmd1(ecat_base,4, 0, 0, 0, 0,  solve.joint_torque[2]); 
                CAN_Handlej.Send_moto_Cmd2(ecat_base,5, 0, 0, 0, 0,  solve.joint_torque[3]); 
                CAN_Handlej.Send_moto_Cmd2(ecat_base,6, 0, 0, 0, 0,  solve.joint_torque[4]); 
                CAN_Handlej.Send_moto_Cmd2(ecat_base,7, 0, 0, 0, 0,  solve.joint_torque[5]);                 
            } else {
                    if(play.is_playing)
                    {
                        limit_joint(target_pos);
                        CAN_Handlej.Send_moto_Cmd1(ecat_base, 1, 150, 12, target_pos[0], 0,     solve.joint_torque[0]   );  
                        CAN_Handlej.Send_moto_Cmd1(ecat_base, 2, 150, 12, target_pos[1], 0,     solve.joint_torque[1]   );  
                        CAN_Handlej.Send_moto_Cmd1(ecat_base, 4, 150, 12, target_pos[2], 0,     solve.joint_torque[2]   );  
                        CAN_Handlej.Send_moto_Cmd2(ecat_base, 5, 30,  1,  target_pos[3], 0,     solve.joint_torque[3]   );  
                        CAN_Handlej.Send_moto_Cmd2(ecat_base, 6, 15,  1,  target_pos[4], 0,     solve.joint_torque[4]   );  
                        CAN_Handlej.Send_moto_Cmd2(ecat_base, 7, 10,  1,  target_pos[5], 0,     solve.joint_torque[5]   );  

                    }
                    else
                    {

                        solve.send_pos(target_pos,target_pos_temp);
                        limit_joint(target_pos_temp);

                        CAN_Handlej.Send_moto_Cmd1(ecat_base, 1, 150, 12, target_pos_temp[0], 0, solve.joint_torque[0]); 
                        CAN_Handlej.Send_moto_Cmd1(ecat_base, 2, 150, 12, target_pos_temp[1], 0, solve.joint_torque[1]); 
                        CAN_Handlej.Send_moto_Cmd1(ecat_base, 4, 150, 12, target_pos_temp[2], 0, solve.joint_torque[2]); 
                        CAN_Handlej.Send_moto_Cmd2(ecat_base, 5, 30, 0.8, target_pos_temp[3], 0, solve.joint_torque[3]); 
                        CAN_Handlej.Send_moto_Cmd2(ecat_base, 6, 25, 0.8, target_pos_temp[4], 0, solve.joint_torque[4]); 
                        CAN_Handlej.Send_moto_Cmd2(ecat_base, 7, 10, 1  , target_pos_temp[5], 0, solve.joint_torque[5]); 
                }
                }

    }else
    {               
                    CAN_Handlej.Send_moto_Cmd1(ecat_base,1, 0, 12, 0, 0, 0); 
                    CAN_Handlej.Send_moto_Cmd1(ecat_base,2, 0, 12, 0, 0, 0); 
                    CAN_Handlej.Send_moto_Cmd1(ecat_base,4, 0, 12, 0, 0, 0); 
                    CAN_Handlej.Send_moto_Cmd2(ecat_base,5, 0, 1, 0, 0, 0);  
                    CAN_Handlej.Send_moto_Cmd2(ecat_base,6, 0, 1, 0, 0, 0);  
                    CAN_Handlej.Send_moto_Cmd2(ecat_base,7, 0, 1, 0, 0, 0);  

                    ROS_WARN("safe mode!!!!!!!!!");

    }

    ecat_base.EcatSyncMsg(); CAN_Handlej.can0_ReceiveFrame(ecat_base);
 
}

void arx_arm::joint_control(ecat::EcatBase ecat_base)
{

    if(current_normal)
    {
        solve.send_pos(ros_control_pos_t,ros_control_pos);
        limit_joint(ros_control_pos);

        CAN_Handlej.Send_moto_Cmd1(ecat_base, 1, 150, 12, ros_control_pos[0], 0, 0); 
        CAN_Handlej.Send_moto_Cmd1(ecat_base, 2, 210, 12, ros_control_pos[1], 0, 0); 
        CAN_Handlej.Send_moto_Cmd1(ecat_base, 4, 150, 12, ros_control_pos[2], 0, 0); 
        CAN_Handlej.Send_moto_Cmd2(ecat_base, 5, 30, 0.8, ros_control_pos[3], 0, 0); 
        CAN_Handlej.Send_moto_Cmd2(ecat_base, 6, 25, 0.8, ros_control_pos[4], 0, 0); 
        CAN_Handlej.Send_moto_Cmd2(ecat_base, 7, 10, 1  , ros_control_pos[5], 0, 0); 
    }
    else
    {

        CAN_Handlej.Send_moto_Cmd1(ecat_base,1, 0, 12, 0, 0, 0);
        CAN_Handlej.Send_moto_Cmd1(ecat_base,2, 0, 12, 0, 0, 0);
        CAN_Handlej.Send_moto_Cmd1(ecat_base,4, 0, 12, 0, 0, 0);
        CAN_Handlej.Send_moto_Cmd2(ecat_base,5, 0, 1, 0, 0, 0); 
        CAN_Handlej.Send_moto_Cmd2(ecat_base,6, 0, 1, 0, 0, 0); 
        CAN_Handlej.Send_moto_Cmd2(ecat_base,7, 0, 1, 0, 0, 0); 
        ROS_WARN("safe mode!!!!!!!!!");

    }
    ecat_base.EcatSyncMsg(); CAN_Handlej.can0_ReceiveFrame(ecat_base);
}


void arx_arm::init_step(ecat::EcatBase ecat_base)
{

    solve.init_pos(target_pos,current_pos,target_pos_temp,is_starting,is_arrived ,teach2pos_returning,temp_init);

    CAN_Handlej.Send_moto_Cmd1(ecat_base,1, 150, 12, target_pos_temp[0], 0,   0 ); 
    CAN_Handlej.Send_moto_Cmd1(ecat_base,2, 150, 12, target_pos_temp[1], 0,   0 ); 
    CAN_Handlej.Send_moto_Cmd1(ecat_base,4, 150, 12, target_pos_temp[2], 0,   0 ); 
    CAN_Handlej.Send_moto_Cmd2(ecat_base,5, 30, 1 ,  target_pos_temp[3], 0,   solve.joint_torque[3] );
    CAN_Handlej.Send_moto_Cmd2(ecat_base,6, 25, 0.8, target_pos_temp[4], 0,   solve.joint_torque[4] );
    CAN_Handlej.Send_moto_Cmd2(ecat_base,7, 10, 1,   target_pos_temp[5], 0,   solve.joint_torque[5] );
    CAN_Handlej.Send_moto_Cmd2(ecat_base,8, 10, 1,   target_pos_temp[6], 0,   0 ); ecat_base.EcatSyncMsg(); CAN_Handlej.can0_ReceiveFrame(ecat_base);               

    cmd_init();
    
    is_teach_mode = false;
    is_torque_control = false;
    current_normal=true;
    ROS_WARN(">>>is_init>>>");
      
}


int arx_arm::rosGetch()
{ 
    static struct termios oldTermios, newTermios;
    tcgetattr( STDIN_FILENO, &oldTermios);          
    newTermios = oldTermios; 
    newTermios.c_lflag &= ~(ICANON);                      
    newTermios.c_cc[VMIN] = 0; 
    newTermios.c_cc[VTIME] = 0;
    tcsetattr( STDIN_FILENO, TCSANOW, &newTermios);  

    int keyValue = getchar(); 

    tcsetattr( STDIN_FILENO, TCSANOW, &oldTermios);  
    return keyValue;
}


void arx_arm::getKey(char key_t) {
   int wait_key=100;

    if(key_t == 'w')
    arx5_cmd.key_x = arx5_cmd.key_x_t=1;
    else if(key_t == 's')
    arx5_cmd.key_x = arx5_cmd.key_x_t=-1;
    else arx5_cmd.key_x_t++;
    if(arx5_cmd.key_x_t>wait_key)
    arx5_cmd.key_x = 0;

    if(key_t == 'a')
    arx5_cmd.key_y =arx5_cmd.key_y_t= 1;
    else if(key_t == 'd')
    arx5_cmd.key_y =arx5_cmd.key_y_t= -1;
    else if(key_t == 'R')
    arx5_cmd.key_y =arx5_cmd.key_y_t= -1;
    else if(key_t == 'L')
    arx5_cmd.key_y =arx5_cmd.key_y_t= 1;
    else arx5_cmd.key_y_t++;
    if(arx5_cmd.key_y_t>wait_key)
    arx5_cmd.key_y = 0;   

    if(key_t == 'U')
    arx5_cmd.key_z =arx5_cmd.key_z_t= 1;
    else if(key_t == 'D')
    arx5_cmd.key_z =arx5_cmd.key_z_t= -1;
    else arx5_cmd.key_z_t++;
    if(arx5_cmd.key_z_t>wait_key)
    arx5_cmd.key_z = 0;

    if(key_t == 'q')
    arx5_cmd.key_base_yaw =arx5_cmd.key_base_yaw_t= 1;
    else if(key_t == 'e')
    arx5_cmd.key_base_yaw =arx5_cmd.key_base_yaw_t= -1;
    else arx5_cmd.key_base_yaw_t++;
    if(arx5_cmd.key_base_yaw_t>wait_key)
    arx5_cmd.key_base_yaw = 0;

    if(key_t == 'r')
    arx5_cmd.key_reset =arx5_cmd.key_reset_t=1;
    else arx5_cmd.key_reset_t++;
    if(arx5_cmd.key_reset_t>wait_key)
    arx5_cmd.key_reset =0;

    if(key_t == 'i')
    arx5_cmd.key_i =arx5_cmd.key_i_t=1;
    else arx5_cmd.key_i_t++;
    if(arx5_cmd.key_i_t>wait_key)
    arx5_cmd.key_i =0;

    if(key_t == 'p')
    arx5_cmd.key_p =arx5_cmd.key_p_t=1;
    else arx5_cmd.key_p_t++;
    if(arx5_cmd.key_p_t>wait_key)
    arx5_cmd.key_p =0;

    if(key_t == 'o')
    arx5_cmd.key_o =arx5_cmd.key_o_t=1;
    else arx5_cmd.key_o_t++;
    if(arx5_cmd.key_o_t>wait_key)
    arx5_cmd.key_o =0;

    if(key_t == 'c')
    arx5_cmd.key_c =arx5_cmd.key_c_t=1;
    else arx5_cmd.key_c_t++;
    if(arx5_cmd.key_c_t>wait_key)
    arx5_cmd.key_c =0;

    if(key_t == 't')
    arx5_cmd.key_t =arx5_cmd.key_t_t=1;
    else arx5_cmd.key_t_t++;
    if(arx5_cmd.key_t_t>wait_key)
    arx5_cmd.key_t =0;

    if(key_t == 'g')
    arx5_cmd.key_g =arx5_cmd.key_g_t=1;
    else arx5_cmd.key_g_t++;
    if(arx5_cmd.key_g_t>wait_key)
    arx5_cmd.key_g =0; 

    if(key_t == 'm')
    arx5_cmd.key_m =arx5_cmd.key_m_t=1;
    else arx5_cmd.key_m_t++;
    if(arx5_cmd.key_m_t>wait_key)
    arx5_cmd.key_m =0;

    if(key_t == 'n')
    arx5_cmd.key_roll =arx5_cmd.key_roll_t= 1;
    else if(key_t == 'm')
    arx5_cmd.key_roll =arx5_cmd.key_roll_t= -1;
    else arx5_cmd.key_roll_t++;
    if(arx5_cmd.key_roll_t>wait_key)
    arx5_cmd.key_roll = 0;  

    if(key_t == 'l')
    arx5_cmd.key_pitch =arx5_cmd.key_pitch_t= 1;
    else if(key_t == '.')
    arx5_cmd.key_pitch =arx5_cmd.key_pitch_t= -1;
    else arx5_cmd.key_pitch_t++;
    if(arx5_cmd.key_pitch_t>wait_key)
    arx5_cmd.key_pitch = 0;  

    if(key_t == ',')
    arx5_cmd.key_yaw =arx5_cmd.key_yaw_t= 1;
    else if(key_t == '/')
    arx5_cmd.key_yaw =arx5_cmd.key_yaw_t= -1;
    else arx5_cmd.key_yaw_t++;
    if(arx5_cmd.key_yaw_t>wait_key)
    arx5_cmd.key_yaw = 0;  

    if(key_t == 'u')
    arx5_cmd.key_u =arx5_cmd.key_u_t=1;
    else arx5_cmd.key_u_t++;
    if(arx5_cmd.key_u_t>wait_key)
    arx5_cmd.key_u =0;

    if(key_t == 'j')
    arx5_cmd.key_j =arx5_cmd.key_j_t=1;
    else arx5_cmd.key_j_t++;
    if(arx5_cmd.key_j_t>wait_key)
    arx5_cmd.key_j =0;

    if(key_t == 'h')
    arx5_cmd.key_h =arx5_cmd.key_h_t=1;
    else arx5_cmd.key_h_t++;
    if(arx5_cmd.key_h_t>wait_key)
    arx5_cmd.key_h =0;

    if(key_t == 'k')
    arx5_cmd.key_k =arx5_cmd.key_k_t=1;
    else arx5_cmd.key_k_t++;
    if(arx5_cmd.key_k_t>wait_key)
    arx5_cmd.key_k =0;

    if(key_t == 'v')
    arx5_cmd.key_v =arx5_cmd.key_v_t=1;
    else arx5_cmd.key_v_t++;
    if(arx5_cmd.key_v_t>wait_key)
    arx5_cmd.key_v =0;

    if(key_t == 'b')
    arx5_cmd.key_b =arx5_cmd.key_b_t=1;
    else arx5_cmd.key_b_t++;
    if(arx5_cmd.key_b_t>wait_key)
    arx5_cmd.key_b =0;
    
    return ;
}

void arx_arm::gripper_control(ecat::EcatBase ecat_base)
{

    if (is_recording) {
            // CAN_Handlej.CAN_GRIPPER(0, 0, 0, 0);
    }
    else if(is_teach_mode)
    {
        CAN_Handlej.Send_moto_Cmd2(ecat_base,8, 0, 0, 0, 0,0 );ecat_base.EcatSyncMsg(); CAN_Handlej.can0_ReceiveFrame(ecat_base);
    }
    else
    {

        if(play.is_playing)
        {
            if( play_gripper)
            gripper_spd=45;
            else if ( !play_gripper) 
            gripper_spd=-45;
            gripper_cout=12*(gripper_spd-rv_motor_msg[8].gripper_spd);
        }else
        {
            if(Teleop_Use()->axes_[2]<0  || arx5_cmd.key_o ) 
            {
                gripper_spd=45;
                gripper_pos[0]+=0.01;
            }
            else if (Teleop_Use()->buttons_[4] == 1 || arx5_cmd.key_c ) 
            {
                gripper_spd=-45;
                gripper_pos[0]-=0.01;
            }

            gripper_cout=12*(gripper_spd-rv_motor_msg[8].gripper_spd);
            if(gripper_pos[0]<-0.1 )
                gripper_pos[0]=-0.1;
            else if(gripper_pos[0]>5)
                gripper_pos[0]=5;

            CAN_Handlej.Send_moto_Cmd2(ecat_base,8, 10, 1,  gripper_pos[0], 0,   0 );ecat_base.EcatSyncMsg(); CAN_Handlej.can0_ReceiveFrame(ecat_base);
        }        

    }



}


void arx_arm::arm_torque_mode()
{
        if( (Teleop_Use()->buttons_[5] == 1 && !button5_pressed)  || ( arx5_cmd.key_i ==1 &&  !button5_pressed) )
        {
            button5_pressed = true;
            if (!is_torque_control) 
            {
                init_kp=10,init_kp_4=20,init_kd=init_kd_4=init_kd_6=init_kp_4=0; 
                is_teach_mode = true;
                is_torque_control = true;
                for (int i = 0; i < 6; i++)
                {
                    prev_target_pos[i] = target_pos[i];
                }
            }else
            {   
                is_teach_mode = false;
                is_torque_control = false;
                teach2pos_returning = true;
                for (int i = 0; i < 6; i++)
                {
                    target_pos[i] = current_pos[i];
                }
                
            }
        }
           
 
        if (button5_pressed && (!Teleop_Use()->buttons_[5] && !arx5_cmd.key_i ))
            button5_pressed = false; 

}

void arx_arm::arm_reset_mode(){
        if((Teleop_Use()->buttons_[1] == 1)|| (arx5_cmd.key_reset==1)) // reset
        {    
            is_starting=1;
        } 
        arx5_cmd.reset = false;
}

void arx_arm::arm_get_pos(){

            arx5_cmd.base_yaw_t += (Teleop_Use()->axes_[0]/100.0f + arx5_cmd.key_base_yaw/100.0f);
            // ROS_INFO("arx5_cmd.base_yaw_t>%f,Teleop_Use()->axes_[0]>%f,arx5_cmd.key_base_yaw>%f",arx5_cmd.base_yaw_t,Teleop_Use()->axes_[0],arx5_cmd.key_base_yaw);
            // 手柄通道+键盘通道+ROS通道  
            if(abs(arx5_ros_cmd.x)<0.1)
                ros_move_k_x=500;
            else ros_move_k_x=100;

            if(abs(arx5_ros_cmd.y)<0.1)
                ros_move_k_y=500;
            else ros_move_k_y=100;

            if(abs(arx5_ros_cmd.z)<0.1)
                ros_move_k_z=500;
            else ros_move_k_z=100;

            float key_kp=1000;
            arx5_cmd.control_x += (joystick_projection(Teleop_Use()->axes_[1])/1000.0f+arx5_cmd.key_x/key_kp  +arx5_ros_cmd.x/ros_move_k_x);
            arx5_cmd.control_y += (joystick_projection(Teleop_Use()->axes_[3])/1000.0f+arx5_cmd.key_y/key_kp  +arx5_ros_cmd.y/ros_move_k_y);
            arx5_cmd.control_z += (joystick_projection(Teleop_Use()->axes_[4])/1000.0f+arx5_cmd.key_z/key_kp  +arx5_ros_cmd.z/ros_move_k_z);

            if (Teleop_Use()->buttons_[2] == 1){ //手柄控制 - 键位组合
            arx5_cmd.control_pit -= (Teleop_Use()->axes_[7]/100.0f+arx5_cmd.key_pitch/1000.0f);
            arx5_cmd.control_yaw   += (Teleop_Use()->axes_[6]/100.0f+arx5_cmd.key_yaw/1000.0f);
            }else
            { //arx5_ros_cmd
            arx5_cmd.control_pit += (arx5_ros_cmd.pitch/1000.0f -arx5_cmd.key_pitch/100.0f);
            arx5_cmd.control_yaw   += (arx5_ros_cmd.yaw/1000.0f   +arx5_cmd.key_yaw  /100.0f);
            }
            arx5_cmd.control_roll  += (-Teleop_Use()->axes_[6]/100.0f-arx5_cmd.key_roll/100.0f + arx5_ros_cmd.roll/1000.0f);
            
            joy_x_t = arx5_cmd.control_x      + magic_pos[0];
            joy_y_t = arx5_cmd.control_y      + magic_pos[1];
            joy_z_t = arx5_cmd.control_z      + magic_pos[2];
            joy_pitch_t=arx5_cmd.control_pit  + magic_angle[0];
            joy_yaw_t  =arx5_cmd.control_yaw  + magic_angle[1];
            joy_roll_t =arx5_cmd.control_roll + magic_angle[2];
            
            //限位
            limit_pos();

            arx5_cmd.reset = true;
            float reset_temp_k=100; //0.001  0.01

            if(control_mode == 0 || control_mode ==1)
            {
                arx5_cmd.x            = ramp(joy_x_t, arx5_cmd.x, reset_temp_k);  
                arx5_cmd.y            = ramp(joy_y_t, arx5_cmd.y, reset_temp_k);
                arx5_cmd.z            = ramp(joy_z_t, arx5_cmd.z, reset_temp_k);
                arx5_cmd.base_yaw     = ramp(arx5_cmd.base_yaw_t, arx5_cmd.base_yaw, 0.009);
                arx5_cmd.waist_roll = ramp(joy_roll_t, arx5_cmd.waist_roll, 0.01);
                arx5_cmd.waist_pitch  = ramp(joy_pitch_t, arx5_cmd.waist_pitch, 0.01);
                arx5_cmd.waist_yaw    = ramp(joy_yaw_t, arx5_cmd.waist_yaw, 0.01);
                arx5_cmd.mode = FORWARD;

            }else if(control_mode == 2 || control_mode ==3)
            {
              
            }

            arx5_cmd.x           = limit<float>(arx5_cmd.x             , lower_bound_waist[0], upper_bound_waist[0]);
            arx5_cmd.y           = limit<float>(arx5_cmd.y             , lower_bound_waist[1], upper_bound_waist[1]);
            arx5_cmd.z           = limit<float>(arx5_cmd.z             , lower_bound_waist[2], upper_bound_waist[2]);
            arx5_cmd.waist_roll  = limit<float>(arx5_cmd.waist_roll    , lower_bound_pitch, upper_bound_pitch);
            arx5_cmd.waist_pitch = limit<float>(arx5_cmd.waist_pitch   , lower_bound_yaw, upper_bound_yaw);
            arx5_cmd.waist_yaw   = limit<float>(arx5_cmd.waist_yaw     , lower_bound_roll, upper_bound_roll);        



        // ROS_INFO(" cmd >>> : J1: %f, J2: %f, J3: %f ,J4: %f, J5: %f, J6: %f ",          arx5_cmd.x,\
        //                                                                                 arx5_cmd.y,\
        //                                                                                 arx5_cmd.z,\
        //                                                                                 arx5_cmd.waist_roll,\
        //                                                                                 arx5_cmd.waist_pitch,\
        //                                                                                 arx5_cmd.waist_yaw\
        //                                                                                 );
        

}

void arx_arm::arm_teach_mode(){

// Record settings
            if (arx5_cmd.key_t == 1 && !button_teach)
            {
                if(!is_recording){
                    is_recording  = true;
                    is_teach_mode = true;
                }
                else
                {
                    is_recording  = false;
                    is_teach_mode = false;
                    out_teach_path = ros::package::getPath("arm_control") + "/teach_record/out.txt";
                    play.end_record(out_teach_path);
                    current_normal = false;
                }
                button_teach = true;
            }
            if (button_teach && !arx5_cmd.key_t)
                button_teach = false;

            if ((arx5_cmd.key_g == 1) && (!is_recording) && (!button_teach))
            // if(1)
            {
                std::vector<std::string> arx;
                play.play_start(ros::package::getPath("arm_control") + "/teach_record/out.txt",target_pos,current_pos,arx);
                button_replay = true;
                play.is_play_initing=true;
            }
            if (button_replay && !arx5_cmd.key_g)
                button_replay = false;
 
}

void arx_arm::limit_pos()
{
        joy_x_t = limit<float>(joy_x_t, lower_bound_waist[0], upper_bound_waist[0]);
        joy_y_t = limit<float>(joy_y_t, lower_bound_waist[1], upper_bound_waist[1]);
        joy_z_t = limit<float>(joy_z_t, lower_bound_waist[2], upper_bound_waist[2]);
        joy_pitch_t = limit<float>(joy_pitch_t, lower_bound_pitch, upper_bound_pitch);
        joy_yaw_t   = limit<float>(joy_yaw_t, lower_bound_yaw, upper_bound_yaw);
        joy_roll_t  = limit<float>(joy_roll_t, lower_bound_roll,upper_bound_roll);

        arx5_cmd.control_x = limit<float>(arx5_cmd.control_x, lower_bound_waist[0], upper_bound_waist[0]);
        arx5_cmd.control_y = limit<float>(arx5_cmd.control_y, lower_bound_waist[1], upper_bound_waist[1]);
        arx5_cmd.control_z = limit<float>(arx5_cmd.control_z, lower_bound_waist[2], upper_bound_waist[2]);
        arx5_cmd.control_pit   = limit<float>(arx5_cmd.control_pit, lower_bound_pitch, upper_bound_pitch);
        arx5_cmd.control_yaw   = limit<float>(arx5_cmd.control_yaw, lower_bound_yaw, upper_bound_yaw);
        arx5_cmd.control_roll  = limit<float>(arx5_cmd.control_roll, lower_bound_roll, upper_bound_roll);

        magic_pos[0] = limit<float>(magic_pos[0], lower_bound_waist[0], upper_bound_waist[0]);
        magic_pos[1] = limit<float>(magic_pos[1], lower_bound_waist[1], upper_bound_waist[1]);
        magic_pos[2] = limit<float>(magic_pos[2], lower_bound_waist[2], upper_bound_waist[2]);
        magic_angle[0] = limit<float>(magic_angle[0], lower_bound_pitch, upper_bound_pitch);
        magic_angle[1] = limit<float>(magic_angle[1], lower_bound_yaw, upper_bound_yaw);
        magic_angle[2] = limit<float>(magic_angle[2], lower_bound_roll, upper_bound_roll);        

}

void arx_arm::cmd_init()
{
    arx5_cmd.waist_pitch  = arx5_cmd.waist_pitch_t  =arx5_cmd.control_pit   = joy_pitch_t  =joy_pitch      =0      ;
    arx5_cmd.x            = arx5_cmd.x_t            =arx5_cmd.control_x   = joy_x_t      =joy_x            =0   ;
    arx5_cmd.y            = arx5_cmd.y_t            =arx5_cmd.control_y  = joy_y_t      =joy_y             =0   ;
    arx5_cmd.z            = arx5_cmd.z_t            =arx5_cmd.control_z   = joy_z_t      =joy_z            =0   ;      
    arx5_cmd.base_yaw     = arx5_cmd.base_yaw_t     =      0 ;
    arx5_cmd.waist_roll = arx5_cmd.waist_roll_t =arx5_cmd.control_roll   = joy_roll_t   =joy_roll       =0      ;
    arx5_cmd.waist_yaw    = arx5_cmd.waist_yaw_t    =arx5_cmd.control_yaw   = joy_yaw_t    =joy_yaw        =0      ;
    arx5_cmd.mode = FORWARD;

}

void arx_arm::limit_joint(float* Set_Pos)
{
        Set_Pos[0] = limit<float>(Set_Pos[0], solve.Lower_Joint[0], solve.Upper_Joint[0]);
        Set_Pos[1] = limit<float>(Set_Pos[1], solve.Lower_Joint[1], solve.Upper_Joint[1]);
        Set_Pos[2] = limit<float>(Set_Pos[2], solve.Lower_Joint[2], solve.Upper_Joint[2]);
        Set_Pos[3] = limit<float>(Set_Pos[3], solve.Lower_Joint[3], solve.Upper_Joint[3]);
        Set_Pos[4] = limit<float>(Set_Pos[4], solve.Lower_Joint[4], solve.Upper_Joint[4]);
        Set_Pos[5] = limit<float>(Set_Pos[5], solve.Lower_Joint[5], solve.Upper_Joint[5]);
}