#ifndef VCD_H_
#define VCD_H_

#include "app.h"
#include "VCDBase.h"
#include <mobilenode.h>
#include <iostream>
#include <timer-handler.h>
#include <cmath>
#include "config.h"
#include <stdlib.h>
#include <time.h>

/**
 * The six message types definition.
 */
typedef enum {REQUEST, TRAFFIC, TRANSFER, CTS, ADVER, FINISH} VCD_Header_t;
/**
 * Transform the enum type to the corresponding Packet Name
 */
const char* pktName(int pkt)
{
    switch (pkt) {
        case 0:
            return "REQUEST ";
        case 1:
            return "TRAFFIC ";
        case 2:
            return "TRANSFER ";
        case 3:
            return "CTS ";
        case 4:
            return "ADVER ";
        case 5:
            return "FINISH ";
        default:
            return "ERROR!!! ";
    }
}

/**
 * The State types definitions.
 */

typedef enum {IDLE_STATE, DOWNLOAD_STATE, COVERAGE_STATE,
              TRANSFER_WAIT_STATE, CTS_WAIT_STATE, TRANSFER_CTS_STATE,
              TRANSFER_SEND_STATE, TRANSFER_RECEIVE_STATE, FINISH_STATE
             } VCD_State_t;
/**
 * Transform the enum type to the corresponding State Name
 */
const char* stateName(int state)
{
    switch (state) {
        case 0:
            return "IDLE_STATE ";
        case 1:
            return "DOWNLOAD_STATE ";
        case 2:
            return "COVERAGE_STATE ";
        case 3:
            return "TRANSFER_WAIT_STATE ";
        case 4:
            return "CTS_WAIT_STATE ";
        case 5:
            return "TRANSFER_CTS_STATE ";
        case 6:
            return "TRANSFER_SEND_STATE ";
        case 7:
            return "TRANSFER_RECEIVE_STATE ";
        case 8:
            return "FINISH_STATE ";
        default:
            return "NULL ";
    }
}

/**
 * Packet State for ADVER and CTS Msg
 */
typedef enum {EMPTY, RECEIVED, WANT, UNWANT} Pkt_State_t;

/**
 * global timing information
 * It is configurable from the OTcl script
 */
#define GPS_INTERVAL InputParameter::gps_interval_
#define PACKET_SIZE InputParameter::packet_size_
#define FILE_SIZE InputParameter::file_size_
#define AP_NUM InputParameter::ap_num_
#define VEHICLE_NUM InputParameter::vehicle_num_
#define RADIO_RANGE InputParameter::radio_range_
#define ADVER_TIMEOUT InputParameter::adver_timeout_
#define TRAFFIC_TIMEOUT InputParameter::traffic_timeout_
#define SHARING_TIMEOUT InputParameter::sharing_timeout_
#define CTS_TIMEOUT InputParameter::cts_timeout_
#define TRANSFER_ACK_TIMEOUT InputParameter::transfer_ack_timeout_
#define CTS_TX_TIMEOUT InputParameter::cts_tx_timeout_

/**
 * some commonly used parameters
 */
#define INVALID -1
#define VALID 1
#define MAXIMUM 2147483647
#define BROADCAST -1
#define CURRENT_TIME Scheduler::instance().clock()

/**
 * msg logging macro function defining
 */
// uncomment the following line, if we do not want log message.

//#define DEBUG_VCD

#ifdef DEBUG_VCD

#define LOG_RX cout << "No." << n_num_ << " Rx " \
                    << pktName((int)data->getType()) << "from No." \
                    << data->getSender() << " "\
                    << stateName(state_) << CURRENT_TIME << endl

#define LOG_OVERHEAR cout << "No." << n_num_ << " Overhear " \
                        << pktName((int)data->getType()) << "from No." \
                        << data->getSender() << " "\
                        << stateName(state_) << CURRENT_TIME << endl

#define LOG_BRX cout << "No." << n_num_ << " Rx Broadcast" \
                    << pktName((int)data->getType()) << "from No." \
                    << data->getSender() << " "\
                    << stateName(state_) << CURRENT_TIME << endl

#define LOG_ERROR_RX cerr << "No." << n_num_ << " Rx " \
                    << pktName((int)data->getType()) << "from No."\
                    << data->getSender() << " "\
                    << stateName(state_) << CURRENT_TIME << endl

#define LOG_ERROR_OVERHEAR cerr << "No." << n_num_ << " Overhear " \
                            << pktName((int)data->getType()) << "from No." \
                            << data->getSender() << " "\
                            << stateName(state_) << CURRENT_TIME << endl

#define LOG_ERROR_BRX cerr << "No." << n_num_ << " Rx Broadcast " \
                        << pktName((int)data->getType()) << "from No."\
                        << data->getSender() << " "\
                        << stateName(state_) << CURRENT_TIME << endl

#define LOG_TIMEOUT(m) cout << "No." << n_num_ << m << "Timeout " \
                        << stateName(state_) << CURRENT_TIME << endl

#define LOG_ERROR_TIMEOUT(m) cerr << "No." << n_num_ << m << "Timeout ERROR!" \
                                << stateName(state_) << CURRENT_TIME << endl

#define LOG_BTX(m) cout << "No." << n_num_ << " Tx Broadcast " << m \
                    << stateName(state_) << CURRENT_TIME << endl

#define LOG_TX(m) cout << "No." << n_num_ << " Tx " << m << "to " \
                    << data->getSender() \
                    << stateName(state_) << CURRENT_TIME << endl

#define LOG_PKT_TX(m,n) cout << "No." << n_num_ << " Tx Pkt No." << m << " to "\
                        << n << " " << stateName(state_) << CURRENT_TIME << endl

#define LOG_TX_CTS cout << "No." << n_num_ << " Tx CTS to " \
                        << transfer_sender_ << stateName(state_) \
                        << CURRENT_TIME << endl

#define LOG_OVERHEAR_DATA cout << "No." << n_num_ << " Overhear " \
                            << pktName((int)data->getType()) << " No." \
                            << data->getSeq() << " from No." \
                            << data->getSender() << " "\
                            << stateName(state_) << CURRENT_TIME << endl

#define LOG_RX_DATA cout << "No." << n_num_ << " Rx " \
                            << pktName((int)data->getType()) << " No." \
                            << data->getSeq() << " from No." \
                            << data->getSender() << " "\
                            << stateName(state_) << CURRENT_TIME << endl

#else

#define LOG_RX
#define LOG_OVERHEAR
#define LOG_BRX
#define LOG_ERROR_RX
#define LOG_ERROR_OVERHEAR
#define LOG_ERROR_BRX
#define LOG_TIMEOUT(m)
#define LOG_ERROR_TIMEOUT(m)
#define LOG_BTX(m)
#define LOG_TX(m)
#define LOG_PKT_TX(m,n)
#define LOG_TX_CTS
#define LOG_OVERHEAR_DATA
#define LOG_RX_DATA

#endif

/**
 * node information transmitted in the REQUEST msg.
 */

typedef struct Node_Info {
    int id_;
    double x_;
    double y_;
    double modulus_; // speed modulus
    double angle_;  // speed angle
}Node_Info_t;

/*--------------------------------------------------------------------------- */
/*--------------------------InputParameter----------------------------------- */
/*--------------------------------------------------------------------------- */
/**
 * Because multiple vehicle receivers are available in TRNASFER_SEND_STATE
 * This struct is used to specify the corresponding receiver for each pkt.
 */

typedef struct Pkt_Tx {
    int id_;
    Pkt_State_t status_;
} Pkt_Tx_t;

class VCDClient;

class VCDServer;

class VCDMsg;

class GroupManager;

/**
 * This class accepts parameters from TCL script
 */

class InputParameter : public TclObject
{
public:
    InputParameter();

    static double gps_interval_;
    static double adver_timeout_;
    static double traffic_timeout_;
    static double sharing_timeout_;
    static double cts_timeout_;
    static double transfer_ack_timeout_;
    static double cts_tx_timeout_;
    static int packet_size_;
    static int file_size_;
    static int ap_num_;
    static int vehicle_num_;
    static double radio_range_;
};

/*--------------------------------------------------------------------------- */
/*--------------------------Timers------------------------------------------- */
/*--------------------------------------------------------------------------- */

/**
 * The constructor of the the following seven timers assigns the application pointer.
 */
class VCDGPSUpdateTimer : public TimerHandler
{
public:
    VCDGPSUpdateTimer(GroupManager *manager) {
        manager_ = manager;
    }

    void expire(Event *);

protected:
    GroupManager *manager_;
};

class VCDClientTrafficTimer : public TimerHandler
{
public:
    VCDClientTrafficTimer(VCDClient *app) {
        app_ = app;
    }

    void expire(Event *);

protected:
    VCDClient *app_;
};

class VCDClientSharingTimer : public TimerHandler
{
public:
    VCDClientSharingTimer(VCDClient *app) {
        app_ = app;
    }

    void expire(Event *);

protected:
    VCDClient *app_;
};

/**
 * This timer is organized to reduce the transfer sharing time.
 * The timeout value is related to the sending moment of ADVER msg.
 * This timeout value is taken randomly from an interval [0, reTX_*TIMEOUT]
 * And if no answer for ADVER msg, the reTX_ will add one.
 */
class VCDClientAdverTimer : public TimerHandler
{
public:
    VCDClientAdverTimer(VCDClient *app) {
        app_ = app;
        re_tx_ = 1;
    }

    void expire(Event *);
    void reset();
    void txMore();
    int getTxTimes() {
        return re_tx_;
    }

    void sched();

protected:
    VCDClient *app_;
    int re_tx_;
};

class VCDClientCTSTimer : public TimerHandler
{
public:
    VCDClientCTSTimer(VCDClient *app) {
        app_ = app;
    }

    void expire(Event *);

protected:
    VCDClient *app_;
};

class VCDClientTransferAckTimer : public TimerHandler
{
public:
    VCDClientTransferAckTimer(VCDClient *app) {
        app_ = app;
    }

    void expire(Event *);

protected:
    VCDClient *app_;
};

class VCDClientCtsTxTimer : public TimerHandler
{
public:
    VCDClientCtsTxTimer(VCDClient *app) {
        app_ = app;
    }

    void expire(Event *);

protected:
    VCDClient *app_;
};

/*--------------------------------------------------------------------------- */
/*--------------------------GroupManager------------------------------------- */
/*--------------------------------------------------------------------------- */

class GroupManager : public TclObject
{
public:
    GroupManager();
    ~GroupManager();
    void start();
    virtual int command(int, const char*const*);

    void GPSUpdate();

    static double calDistance(int, int);
    static void updateLoc(MobileNode*, int);

    int* group_header_;
    static VCDClient** vcd_list_;
    static Node_Info_t* pos_info_;
    static MobileNode** node_list_;

private:
    VCDGPSUpdateTimer update_timer_;
};

/*--------------------------------------------------------------------------- */
/*--------------------------VCDClient---------------------------------------- */
/*--------------------------------------------------------------------------- */

class VCDClient : public Application, public VCDBase
{
public:
    VCDClient();
    ~VCDClient();

    virtual int command(int, const char*const*);
    virtual void process_data(int, AppData *);
    virtual void correctSend();
    virtual void inCorrectSend();
    void start();
    void stop();

    void TrafficTimeout();
    void TransferTimeout();
    void AdverTimeout();
    void CTSTimeout();
    void TransferAckTimeout();
    void CtsTxTimeout();

    void updateAttachedAP(int);
    int getAttachedAP();
    void updateState(VCD_State_t);
    VCD_State_t getState();
    void setTransferSender(int);
    int getTransferSender();
    Node_Info_t info();

private:

    VCDClientAdverTimer Adver_timer_;
    VCDClientCTSTimer CTS_timer_;
    VCDClientTrafficTimer Traffic_timer_;
    VCDClientSharingTimer Sharing_timer_;
    VCDClientTransferAckTimer TransferAck_timer_;
    VCDClientCtsTxTimer Cts_tx_timer_;

    VCD_State_t state_;
    int n_num_;
    int attached_AP_;
    int transfer_No; // the current pkt No. for sending TRANSFER msg
    int transfer_sender_; // TRANSFER pkt source for TRANSFER_RECEIVE_STATE

    Pkt_State_t* pkt_ACK_; // local pkt statistics
    Pkt_State_t* pkt_I_need_; // transmitted in CTS message
    Pkt_Tx_t* pkt_other_need_; // collecting from different mess

    void IdleProcess(VCDMsg*);
    void CoverageProcess(VCDMsg*);
    void DownloadProcess(VCDMsg*);
    void TransferWaitProcess(VCDMsg*);
    void CtsWaitProcess(VCDMsg*);
    void TransferCtsProcess(VCDMsg*);
    void TransferReceiveProcess(VCDMsg*);
    void TransferSendProcess(VCDMsg*);

    void sendRequest(int);
    void sendAdver();
    void sendCTS();
    void transferTx();
    int checkFinish();
    void CtsTxTimerSetter();
    void updateOtherNeed(VCDMsg*);
    void cancelAllTimers();
};

/*--------------------------------------------------------------------------- */
/*--------------------------VCDServer---------------------------------------- */
/*--------------------------------------------------------------------------- */

class VCDServer : public Application, public VCDBase
{
public:
    VCDServer();

    virtual int command(int, const char*const*);
    virtual void process_data(int, AppData *);
    virtual void correctSend();
    virtual void inCorrectSend();

    void start();
    void stop();

private:
    VCD_State_t state_;

    int n_num_;
    int sending_n_num_;
    int seqno_; // the previous successfully sent packet no.

    void IdleProcess(VCDMsg*);
};

/*--------------------------------------------------------------------------- */
/*--------------------------VCDMsg------------------------------------------- */
/*--------------------------------------------------------------------------- */

class VCDMsg : public AppData
{

public:
    /**
     * The constructor builds the message.
     * Because not all the message needs the same fields, there are several constructors with different number of parameters.
     */
    VCDMsg(VCD_Header_t type, int sender, int receiver, int seq) :
            AppData(VCD_DATA) {
        if (type == REQUEST) {
            type_ = type;
            sender_ = sender;
            receiver_ = receiver;
            seqno_ = seq;
            payload_size_ = INVALID;
            pkt_status_size_ = 0;
        } else {
            cout << "VCDMsg build error!\n" << endl;
            exit(1);
        }
    }

    VCDMsg(VCD_Header_t type, int sender, int receiver, int size, int seq) :
            AppData(VCD_DATA) {
        if (type == TRAFFIC || TRANSFER) {
            type_ = type;
            sender_ = sender;
            receiver_ = receiver;
            seqno_ = seq;
            payload_size_ = size;
            pkt_status_size_ = 0;
        } else {
            cout << "VCDMsg build error!\n" << endl;
            exit(1);
        }
    }

    VCDMsg(VCD_Header_t type, int sender, Pkt_State_t* pkt) : AppData(VCD_DATA)
{
        if (type == ADVER) {
            type_ = type;
            sender_ = sender;
            receiver_ = BROADCAST;
            seqno_ = INVALID;
            payload_size_ = INVALID;
            pkt_ = pkt;
            pkt_status_size_ = pktStatusSize();
        } else {
            cout << "VCDMsg build error!\n" << endl;
            exit(1);
        }
    }

    VCDMsg(VCD_Header_t type, int sender, int receiver, Pkt_State_t* pkt) :
            AppData(VCD_DATA) {
        if (type == CTS) {
            type_ = type;
            sender_ = sender;
            receiver_ = receiver;
            seqno_ = INVALID;
            payload_size_ = INVALID;
            pkt_ = pkt;
            pkt_status_size_ = pktStatusSize();
        } else {
            cout << "VCDMsg build error!\n" << endl;
            exit(1);
        }
    }

    VCDMsg(VCD_Header_t type, int sender) : AppData(VCD_DATA) {
        if (type == FINISH) {
            type_ = FINISH;
            sender_ = sender;
            receiver_ = INVALID;
            seqno_ = INVALID;
            payload_size_ = INVALID;
        } else {
            cout << "VCDMsg build error!\n" << endl;
            exit(1);
        }
    }

    VCD_Header_t getType() {
        return (type_);
    }

    int getSeq() {
        return seqno_;
    }

    int getSender() {
        return sender_;
    }

    int getReceiver() {
        return receiver_;
    }

    Pkt_State_t* getPkt() {
        return pkt_;
    }

    // override that of the base class AppData
    virtual AppData *copy() {
        return (new VCDMsg(*this));
    }

    // packet has different size according to the packet type
    virtual int size() const {
        switch (type_) {
            case REQUEST:
                // header + group msg information
                return sizeof(VCD_Header_t) + sizeof(Node_Info_t) * VEHICLE_NUM;
            case TRAFFIC:
                // header + sender + receiver + seq no + payload
                return sizeof(VCD_Header_t) + sizeof(int)*3 + payload_size_;
            case TRANSFER:
                // header + sender + receiver + seq no + payload
                return sizeof(VCD_Header_t) + sizeof(int)*3 + payload_size_;
            case ADVER:
                // header + sender + pkt status size
                return sizeof(VCD_Header_t) + sizeof(int) + pkt_status_size_;
            case CTS:
                // header + sender + receiver + pkt status size
                return sizeof(VCD_Header_t) + sizeof(int)*2 + pkt_status_size_;
            case FINISH:
                // header + sender
                return sizeof(VCD_Header_t) + sizeof(int);
            default:
                return sizeof(VCDMsg);
        }
    }

    // compute the size after compression
    int pktStatusSize();

private:
    // In the simulation, the packet status pointer is passed.
    // Because in NS2, the simulator does not use sizeof(PKT) to determine the
    // packet size. The size is specified in the header. So we set the size
    // to the number after compression.
    Pkt_State_t* pkt_;
    VCD_Header_t type_;
    int sender_;
    int receiver_;
    int payload_size_;
    int seqno_;
    int pkt_status_size_;
};

#endif /*VCD_H_*/
