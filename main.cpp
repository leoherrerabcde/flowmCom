#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <algorithm>

#include "SCCFlowProtocol.h"

#include "../commPort/SCCCommPort.h"
#include "../commPort/SCCRealTime.h"
#include "../commPort/SCCArgumentParser.h"

#include "../main_control/CSocket.h"
#include "../main_control/SCCDeviceNames.h"
#include "../main_control/SCCLog.h"

using namespace std;

static bool st_bSendMsgView = true;
static bool st_bRcvMsgView  = true;

CSocket sckComPort;
SCCLog glLog(std::cout);

bool bConnected     = false;

std::string firstMessage()
{
    std::stringstream ss;

    ss << FRAME_START_MARK;
    ss << DEVICE_NAME << ":" << DEVICE_RFID_BOQUILLA << ",";
    ss << SERVICE_PID << ":" << getpid();
    ss << FRAME_STOP_MARK;

    return std::string(ss.str());
}

void printMsg(std::string msg)
{
    if (sckComPort.isConnected())
    {
        if (bConnected == false)
        {
            bConnected = true;
            sckComPort.sendData(firstMessage());
            std::cout << "Socket connected." << std::endl;
        }
        sckComPort.sendData(msg);
    }
    else
    {
        std::cout << SCCRealTime::getTimeStamp() << ',' << msg << std::endl;
    }
}

int main(int argc, char* argv[])
{
    int nPort           = 7;
    int baudRate        = 9600;
    float fTimeFactor   = 1.0;
    int remotePort      = 0;
    int startReg        = 0;
    int numRegs         = MAX_REGISTERS;

    if (argc > 2)
    {
        nPort = std::stoi(argv[1]);
        baudRate = std::stoi(argv[2]);
        if (argc > 3)
            remotePort = std::stoi(argv[3]);
        if (argc > 4)
        {
            std::string strArg(argv[4]);
            if (std::all_of(strArg.begin(), strArg.end(), ::isdigit))
                fTimeFactor = std::stof(argv[4]);
            else
                if (strArg == "ViewSend")
                    st_bSendMsgView = true;
        }
        if ( argc > 6)
        {
            startReg    = std::stoi(argv[5]);
            numRegs     = std::stoi(argv[6]);
        }
    }

    if (remotePort)
    {
        sckComPort.connect("127.0.0.1", remotePort);
        bConnected  = false;
    }
    SCCCommPort commPort;
    SCCFlowProtocol flowProtocol;
    SCCRealTime clock;

    commPort.openPort(nPort, baudRate);

    char bufferOut[255];
    char bufferIn[250];
    char len;
    int iAddr = 1;
    std::string msg;

    //msg = flowProtocol.getStrCmdStatusCheck(iAddr, bufferOut, len);
    msg = flowProtocol.getCmdReadRegisters(iAddr, bufferOut, len, startReg, numRegs);

    //flowProtocol.getCmdReadRegisters(iAddr, bufferOut, len);

    msg = flowProtocol.convChar2Hex(bufferOut, len);

    if (st_bSendMsgView)
        std::cout << "Message: " << bufferOut << " sent." << std::endl;
    commPort.sendData(bufferOut, len);

    if (st_bSendMsgView)
        cout << "Waiting for response" << std::endl;
    msg = "";

    int iTimeOut;
    bool bNextAddr;
    char chLen = 0;
    //char chLenLast = 0;
    int iNoRxCounter = 0;
    do
    {
        bNextAddr = true;
        iTimeOut = 1000;
        if (iNoRxCounter >= 5)
        {
            iNoRxCounter = 0;
            //flowProtocol.getStrCmdStatusCheck(iAddr, bufferOut, chLen);
            flowProtocol.getCmdReadRegisters(iAddr, bufferOut, chLen, startReg, numRegs);
        }
        if (chLen > 0)
        {
            if (st_bSendMsgView)
            {
                cout << commPort.printCounter() << std::endl;
                msg = flowProtocol.convChar2Hex(bufferOut, chLen);
                cout << SCCRealTime::getTimeStamp() << ',' << "Sending Message: " << bufferOut << std::endl;
            }
            commPort.sendData(bufferOut, chLen);
            //chLenLast = chLen;
            chLen = 0;
            iTimeOut = 20;
            bNextAddr = false;
        }
        if (commPort.isRxEvent() == true)
        {
            iNoRxCounter = 0;
            bNextAddr =false;
            int iLen;
            bool ret = commPort.getData(bufferIn, iLen);
            cout << " bufferIn.len(): " << iLen << ". bufferIn(char): [" << bufferIn << "]" << std::endl;

            if (ret == true)
            {
                len = (char)iLen;
                /*if (st_bRcvMsgView)
                {
                    msg = flowProtocol.convChar2Hex(bufferIn, len);
                    cout << ++nCount << " Buffer In(Hex): [" << msg << "]. Buffer In(char): [" << bufferIn << "]" << std::endl;
                }*/
                std::string strCmd;
                //char resp[256];
                int addr = 0;
                //char respLen = 0;
                bool bIsValidResponse = flowProtocol.getFlowMeterResponse(addr, bufferIn, len);
                //bool bNextAction = false;
                if (bIsValidResponse == true)
                {
                    if (st_bRcvMsgView)
                    {
                        //cout << ++nCount << " " << commPort.printCounter() << clock.getTimeStamp() << " Valid WGT Response" << std::endl;
                        /*if (strCmd == CMD_CHECKSTATUS)
                            cout << ++nCount << " WGT Status: " << flowProtocol.getStrStatus(resp[0]) << endl;*/
                    }
                    /*bNextAction = flowProtocol.nextAction(iAddr, bufferOut, chLen, iTimeOut);
                    if (bNextAction == true)*/
                        if (st_bRcvMsgView)
                        {
                            std::stringstream ss;
                            //ss << ++nCount << " " << commPort.printCounter() << flowProtocol.printStatus(iAddr) << std::endl;
                            printMsg(flowProtocol.printStatus(iAddr));
                        }
                }
            }
        }
        if (iTimeOut > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(iTimeOut*fTimeFactor)));
        if (bNextAddr == true)
        {
            //++iAddr;
        }
        if (/*bConnecting ==true || */bConnected == true)
        {
            if (sckComPort.getState() == sckError)
            {
                if (remotePort)
                {
                    sckComPort.connect("127.0.0.1", remotePort);
                    //bConnecting = true;
                    bConnected  = false;
                    //iSckCounter = 0;
                }
            }
            /*if (sckComPort.isConnected())
            {
                //bConnecting = false;
                bConnected  = true;
            }*/
        }
        ++iNoRxCounter;
        if (bConnected == true && !sckComPort.isConnected())
            break;
    }
    while (commPort.isOpened());

    sckComPort.disconnect();
    commPort.closePort();

    return 0;
}
