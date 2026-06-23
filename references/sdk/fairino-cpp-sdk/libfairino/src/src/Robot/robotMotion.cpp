#include "robot.h"
#include "robot_types.h"
#include "robot_error.h"
#include "logger.h"
#include "XmlRpc.h"
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <FrameHandle.h>
#include "FRUdpClient.h"

using namespace std;
using namespace XmlRpc;


/**
 * @brief 定点摆动开始
 * @param [in] weaveNum 摆动编号[0-7]
 * @param [in] mode 0-工具坐标系；1-参考点
 * @param [in] refPoint 参考点笛卡尔坐标[x,y,z,a,b,c]
 * @param [in] weaveTime 摆动时间[s]
 * @return 错误码
 */
errno_t FRRobot::OriginPointWeaveStart(int weaveNum, int mode, DescPose refPoint, double weaveTime)
{
    if (IsSockError())
    {
        return g_sock_com_err;
    }

    int errcode = 0;
    XmlRpcClient c(serverUrl, 20003);
    XmlRpcValue param, result;

    param[0][0] = weaveNum;
    param[0][1] = mode;
    param[0][2] = refPoint.tran.x;
    param[0][3] = refPoint.tran.y;
    param[0][4] = refPoint.tran.z;
    param[0][5] = refPoint.rpy.rx;
    param[0][6] = refPoint.rpy.ry;
    param[0][7] = refPoint.rpy.rz;
    param[0][8] = weaveTime;

    if (c.execute("OriginPointWeaveStart", param, result))
    {
        errcode = int(result);
        if (0 != errcode)
        {
            logger_error("execute OriginPointWeaveStart fail: %d.", errcode);
            c.close();
            return errcode;
        }
    }
    else
    {
        c.close();
        return ERR_XMLRPC_CMD_FAILED;
    }

    c.close();
    return errcode;
}

/**
 * @brief 定点摆动结束
 * @return 错误码
 */
errno_t FRRobot::OriginPointWeaveEnd()
{
    if (IsSockError())
    {
        return g_sock_com_err;
    }

    int errcode = 0;
    XmlRpcClient c(serverUrl, 20003);
    XmlRpcValue param, result;

    if (c.execute("OriginPointWeaveEnd", param, result))
    {
        errcode = int(result);
        if (0 != errcode)
        {
            logger_error("execute OriginPointWeaveEnd fail: %d.", errcode);
            c.close();
            return errcode;
        }
    }
    else
    {
        c.close();
        return ERR_XMLRPC_CMD_FAILED;
    }

    c.close();
    return errcode;
}

/**
 * @brief 关节空间速度伺服模式运动
 * @param [in] joint_pos 6个目标关节速度,单位deg/s
 * @param [in] axisPos 4个外部轴速度,单位deg/s
 * @param [in] acc 加速度百分比，范围[0~100],暂不开放，默认为0
 * @param [in] vel 速度百分比，范围[0~100]，暂不开放，默认为0
 * @param [in] cmdT 指令下发周期，单位s，建议范围[0.001~0.0016]
 * @param [in] filterT 滤波时间，单位s，暂不开放，默认为0
 * @param [in] gain 目标位置的比例放大器，暂不开放，默认为0
 * @param [in] id servoJ指令ID,默认为0
 * @param [in] comType 指令下发类型；0-xmlrpc；1-UDP(对应机器人20007端口)
 * @return 错误码
 */
errno_t FRRobot::ServoJV(double jointVel[6], double exisVel[4], float acc, float vel, float cmdT, float filterT, float gain, int id, int comType)
{
    if (IsSockError())
    {
        return g_sock_com_err;
    }
    if (GetSafetyCode() != 0)
    {
        return GetSafetyCode();
    }

    if (comType == 0)
    {
        int errcode = 0;
        XmlRpcClient c(serverUrl, 20003);
        XmlRpcValue param, result;

        param[0][0] = jointVel[0];
        param[0][1] = jointVel[1];
        param[0][2] = jointVel[2];
        param[0][3] = jointVel[3];
        param[0][4] = jointVel[4];
        param[0][5] = jointVel[5];
        param[1][0] = exisVel[0];
        param[1][1] = exisVel[1];
        param[1][2] = exisVel[2];
        param[1][3] = exisVel[3];
        param[2] = acc;
        param[3] = vel;
        param[4] = cmdT;
        param[5] = filterT;
        param[6] = gain;
        param[7] = id;

        if (c.execute("ServoJV", param, result))
        {
            errcode = int(result);
            if (0 != errcode)
            {
                logger_error("execute ServoJV fail: %d.", errcode);
                c.close();
                return errcode;
            }
        }
        else
        {
            c.close();
            return ERR_XMLRPC_CMD_FAILED;
        }

        c.close();
        return errcode;
    }
    else if (comType == 1)
    {
        char jointVelStr[128] = { 0 };
        snprintf(jointVelStr, 128, "{%.3f,%.3f,%.3f,%.3f,%.3f,%.3f}", jointVel[0], jointVel[1], jointVel[2], jointVel[3], jointVel[4], jointVel[5]);
        char exaxisVelStr[128] = { 0 };
        snprintf(exaxisVelStr, 128, "{%.3f,%.3f,%.3f,%.3f}", exisVel[0], exisVel[1], exisVel[2], exisVel[3]);
                                                                             
        string cmdStr = string("ServoJV(") + jointVelStr + "," + exaxisVelStr + "," + 
        to_string(acc) + "," + to_string(vel) + "," + to_string(cmdT) + "," + to_string(filterT) + "," + to_string(gain) + "," + to_string(id) + ")";
        FRAME frame(cmdFrameCnt, 1337, cmdStr);
        int rtn = udpCmdClient->SendFrame(PackFrame(frame));
        if (rtn != 0)
        {
            return ERR_SOCKET_SEND_FAILED;
        }
        cmdFrameCnt++;

        return ERR_SUCCESS;
    }
    else
    {
        return ERR_PARAM_VALUE;
    }

}

/**
 * @brief 关节MIT控制开始
 * @param [in]  comType 指令下发类型；0-xmlrpc；1-UDP(对应机器人20007端口)
 * @return  错误码
 */
errno_t FRRobot::ServoMITStart(int comType)
{
    if (IsSockError())
    {
        return g_sock_com_err;
    }
    if (GetSafetyCode() != 0)
    {
        return GetSafetyCode();
    }

    int errcode = 0;

    if (comType == 0)
    {
        XmlRpcClient c(serverUrl, 20003);
        XmlRpcValue param, result;

        if (c.execute("ServoMITStart", param, result))
        {
            errcode = int(result);
            if (0 != errcode)
            {
                logger_error("execute ServoMITStart fail: %d.", errcode);
                c.close();
                return errcode;
            }
        }
        else
        {
            c.close();
            return ERR_XMLRPC_CMD_FAILED;
        }

        c.close();
    }
    else if (comType == 1)
    {
        string cmdStr = string("ServoMITStart()");
        FRAME frame(cmdFrameCnt, 1334, cmdStr);
        int rtn = udpCmdClient->SendFrame(PackFrame(frame));
        if (rtn != 0)
        {
            return ERR_SOCKET_SEND_FAILED;
        }
        cmdFrameCnt++;
    }
    else
    {
        return ERR_PARAM_VALUE;
    }

    return errcode;
}


/**
 * @brief 关节MIT控制结束
 * @param [in]  comType 指令下发类型；0-xmlrpc；1-UDP(对应机器人20007端口)
 * @return  错误码
 */
errno_t FRRobot::ServoMITEnd(int comType)
{
    if (IsSockError())
    {
        return g_sock_com_err;
    }
    if (GetSafetyCode() != 0)
    {
        return GetSafetyCode();
    }

    int errcode = 0;

    if (comType == 0)
    {
        XmlRpcClient c(serverUrl, 20003);
        XmlRpcValue param, result;

        if (c.execute("ServoMITEnd", param, result))
        {
            errcode = int(result);
            if (0 != errcode)
            {
                logger_error("execute ServoMITEnd fail: %d.", errcode);
                c.close();
                return errcode;
            }
        }
        else
        {
            c.close();
            return ERR_XMLRPC_CMD_FAILED;
        }

        c.close();
    }
    else if (comType == 1)
    {
        string cmdStr = string("ServoMITEnd()");
        FRAME frame(cmdFrameCnt, 1335, cmdStr);
        int rtn = udpCmdClient->SendFrame(PackFrame(frame));
        if (rtn != 0)
        {
            return ERR_SOCKET_SEND_FAILED;
        }
        cmdFrameCnt++;
    }
    else
    {
        return ERR_PARAM_VALUE;
    }

    return errcode;
}


/**
* @brief 关节MIT控制
* @param [in] posGain j1~j6关节位置增益
* @param [in] desPos j1~j6关节期望位置 单位:deg
* @param [in] velGain j1~j6关节速度增益
* @param [in] desVel j1~j6关节期望速度 单位:deg/s
* @param [in] torque_ff j1~j6前馈力矩 单位:Nm
* @param [in] interval 指令周期，单位s，范围[0.001~0.008]
* @param [in]  comType 指令下发类型；0-xmlrpc；1-UDP(对应机器人20007端口)
* @return 错误码
*/
errno_t FRRobot::ServoMIT(double posGain[6], double desPos[6], double velGain[6], double desVel[6], double torque_ff[6], double interval, int comType)
{
    if (IsSockError())
    {
        return g_sock_com_err;
    }
    if (GetSafetyCode() != 0)
    {
        return GetSafetyCode();
    }

    if (comType == 0)
    {
        int errcode = 0;
        XmlRpcClient c(serverUrl, 20003);
        XmlRpcValue param, result;

        param[0][0] = posGain[0];
        param[0][1] = posGain[1];
        param[0][2] = posGain[2];
        param[0][3] = posGain[3];
        param[0][4] = posGain[4];
        param[0][5] = posGain[5];
        param[1][0] = desPos[0];
        param[1][1] = desPos[1];
        param[1][2] = desPos[2];
        param[1][3] = desPos[3];
        param[1][4] = desPos[4];
        param[1][5] = desPos[5];
        param[2][0] = velGain[0];
        param[2][1] = velGain[1];
        param[2][2] = velGain[2];
        param[2][3] = velGain[3];
        param[2][4] = velGain[4];
        param[2][5] = velGain[5];
        param[3][0] = desVel[0];
        param[3][1] = desVel[1];
        param[3][2] = desVel[2];
        param[3][3] = desVel[3];
        param[3][4] = desVel[4];
        param[3][5] = desVel[5];
        param[4][0] = torque_ff[0];
        param[4][1] = torque_ff[1];
        param[4][2] = torque_ff[2];
        param[4][3] = torque_ff[3];
        param[4][4] = torque_ff[4];
        param[4][5] = torque_ff[5];
        param[5] = interval;

        if (c.execute("ServoMIT", param, result))
        {
            errcode = int(result);
            if (0 != errcode)
            {
                logger_error("execute ServoMIT fail: %d.", errcode);
                c.close();
                return errcode;
            }
        }
        else
        {
            c.close();
            return ERR_XMLRPC_CMD_FAILED;
        }

        c.close();
        return errcode;
    }
    else if (comType == 1)
    {
        char posGainStr[128] = { 0 };
        snprintf(posGainStr, 128, "{%.3f,%.3f,%.3f,%.3f,%.3f,%.3f}", posGain[0], posGain[1], posGain[2], posGain[3], posGain[4], posGain[5]);
        char desPosStr[128] = { 0 };
        snprintf(desPosStr, 128, "{%.3f,%.3f,%.3f,%.3f,%.3f,%.3f}", desPos[0], desPos[1], desPos[2], desPos[3], desPos[4], desPos[5]);
        char velGainStr[128] = { 0 };
        snprintf(velGainStr, 128, "{%.3f,%.3f,%.3f,%.3f,%.3f,%.3f}", velGain[0], velGain[1], velGain[2], velGain[3], velGain[4], velGain[5]);
        char desVelStr[128] = { 0 };
        snprintf(desVelStr, 128, "{%.3f,%.3f,%.3f,%.3f,%.3f,%.3f}", desVel[0], desVel[1], desVel[2], desVel[3], desVel[4], desVel[5]);
        char torque_ffStr[128] = { 0 };
        snprintf(torque_ffStr, 128, "{%.3f,%.3f,%.3f,%.3f,%.3f,%.3f}", torque_ff[0], torque_ff[1], torque_ff[2], torque_ff[3], torque_ff[4], torque_ff[5]);


        string cmdStr = string("ServoMIT(") + posGainStr + "," + desPosStr + "," + velGainStr + "," + desVelStr + "," + torque_ffStr + "," + to_string(interval) + ")";
        FRAME frame(cmdFrameCnt, 1336, cmdStr);
        int rtn = udpCmdClient->SendFrame(PackFrame(frame));
        if (rtn != 0)
        {
            return ERR_SOCKET_SEND_FAILED;
        }
        cmdFrameCnt++;

        return ERR_SUCCESS;
    }
    else
    {
        return ERR_PARAM_VALUE;
    }

}