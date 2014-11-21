#include "VCD.h"

VCDClient** GroupManager::vcd_list_;
Node_Info_t* GroupManager::pos_info_;
MobileNode** GroupManager::node_list_;

/**
 * The following lines set the default value of the parameters.
 * All values are set to -1 to force the script to set the value.
 */
double InputParameter::gps_interval_ = -1;
double InputParameter::adver_timeout_ = -1;
double InputParameter::traffic_timeout_ = -1;
double InputParameter::sharing_timeout_ = -1;
double InputParameter::cts_timeout_ = -1;
double InputParameter::transfer_ack_timeout_ = -1;
double InputParameter::cts_tx_timeout_ = -1;
int InputParameter::packet_size_ = -1;
int InputParameter::file_size_ = -1;
int InputParameter::ap_num_ = -1;
int InputParameter::vehicle_num_ = -1;
double InputParameter::radio_range_ = -1;

/*--------------------------------------------------------------------------- */
/*--------------------------VCDClient---------------------------------------- */
/*--------------------------------------------------------------------------- */
/**
 * VCDClientAppClass is used to create VCDClient in C++ domain when the user invokes the command in the OTcl domain. 
 * It is necessary when we have to add a new application.
 */
static class VCDClientAppClass : public TclClass
{
public:
    VCDClientAppClass() : TclClass("Application/VCDClient") {}

    TclObject* create(int, const char*const*) {
        return (new VCDClient);
    }
} class_app_VCDClient;

/**
 * The constructor of VCDClient. 
 * It binds the n_num_ variable with the id_ variable in OTcl domain. 
 *
 * It initialzes all the internal variables including six timers, three lists, state and so on. 
 *
 * It allocates the memory space for the lists and assigns initial values.
 *
 * The six timers are C++ class. Their constructors need a pointer to VCDClient.
 */
VCDClient::VCDClient() : Adver_timer_(this), CTS_timer_(this),
        Traffic_timer_(this), Sharing_timer_(this), TransferAck_timer_(this),
        Cts_tx_timer_(this)
{
    // initialization
    bind("id_", &n_num_);

    pkt_ACK_ = (Pkt_State_t*) malloc(sizeof(Pkt_State_t) * FILE_SIZE);
    memset(pkt_ACK_, EMPTY, sizeof(Pkt_State_t) *FILE_SIZE);

    pkt_I_need_ = (Pkt_State_t*) malloc(sizeof(Pkt_State_t) * FILE_SIZE);
    memset(pkt_I_need_, EMPTY, sizeof(Pkt_State_t) *FILE_SIZE);

    pkt_other_need_ = (Pkt_Tx_t*) malloc(sizeof(Pkt_Tx_t) * FILE_SIZE);
    memset(pkt_other_need_, EMPTY, sizeof(Pkt_Tx_t) * FILE_SIZE);

    state_ = IDLE_STATE;
    transfer_sender_ = INVALID;
    transfer_No = INVALID;
    attached_AP_ = INVALID;
}

/**
 * It is the destructor of the VCDClient.
 * It frees the memory spaces for the lists.
 */
VCDClient::~VCDClient()
{
    free(pkt_ACK_);
    free(pkt_I_need_);
    free(pkt_other_need_);
}

/**
 * It receives commands from OTcl domain.
 * @param argc:integer. The number of command line arguments.
 * @param argv:char**. The full list of all of the command line arguments.
 * 
 * The function only accepts "sendRequest" command. 
 * It firstly tests whether the 2nd argument is "sendReqeust" or not. If it is, it invokes the "sendRequest" using the 3rd argument as the input parameter for sendRequest function.
 */
int VCDClient::command(int argc, const char*const* argv)
{
    // because entering coverage of the AP is simulated as the distance
    // only the group manager only that the node is inside the range
    // So this command is mainly used for GroupManager to initialize
    // the transmission of REQUEST msg.
    if (argc == 3) {
        if (strcmp(argv[1], "sendRequest") == 0) {
            sendRequest(atoi(argv[2]));
            return(TCL_OK);
        }
    }

    return (Application::command(argc, argv));
}

/**
 * The function is invoked in the first place when the simulation begins. 
 * It assgins the information of the VCDClient in the GroupManager::vcd_list_ and GroupManager::node_list_.
 */
void VCDClient::start()
{
    // put the pointer into the GroupManager VCDClient pointer list
    // NOTE GroupManager should be initialized before VCDClient
    //cout << "VCDClient No." << n_num_ << " starts!" << endl;
    *(GroupManager::vcd_list_ + n_num_) = this;
    *(GroupManager::node_list_ + n_num_) = (MobileNode*)
        Node::get_node_by_address(agent_->addr());
}

/**
 * The function is invoked when the simulation ends.
 * It counts the number of packets that the client receives and the current state of the client.
 */
void VCDClient::stop()
{
    int i = 0;

    cout << "---- Client NO." << n_num_ << " stops ----- " << endl;

    // packets statistics
    int status = 0;

    for (int j = 0; j < FILE_SIZE; j++) {
        if (* (pkt_ACK_ + j) == RECEIVED) {
            i++;

            if (status == 0) {
                cout << j << endl;
                status = 1;
            }
        } else {
            if (status == 1) {
                cout << j << endl;
                status = 0;
            }
        }
    }

    cout << "state is " << stateName(state_) << endl;

    cout << "It received " << i << " packets." << endl << endl;
}

/**
 * It is the main packet receive function. It is called by the lower transport agent. In our implementation, it is the MessagePassing Agent.
 * Different function is called depending on the current state.
 *
 * @param recvData:AppData. The application data is stored in this pointer
 * @param size:int. The size of the application data.
 * 
 */
void VCDClient::process_data(int size, AppData* recvData)
{
    VCDMsg* data = (VCDMsg *) recvData;

    switch (state_) {
        case IDLE_STATE:
            IdleProcess(data);
            break;
        case COVERAGE_STATE:
            CoverageProcess(data);
            break;
        case DOWNLOAD_STATE:
            DownloadProcess(data);
            break;
        case TRANSFER_WAIT_STATE:
            TransferWaitProcess(data);
            break;
        case CTS_WAIT_STATE:
            CtsWaitProcess(data);
            break;
        case TRANSFER_CTS_STATE:
            TransferCtsProcess(data);
            break;
        case TRANSFER_SEND_STATE:
            TransferSendProcess(data);
            break;
        case TRANSFER_RECEIVE_STATE:
            TransferReceiveProcess(data);
            break;
        case FINISH_STATE:
            break;
        default:
            break;
    }
}

/*---------------------------------------------------------*/
/**
 * Main State Transition Structure
 * if (BROADCAST MSG) {
 *   do something according to pkt type
 * }
 * else if (OVERHEAR MSG) {
 *   do something according to pkt type
 * }
 * else (MSG to this node) {
 *   do something according to pkt type
 * }
 *
 * The followings are the state handling functions.
 * The operations according to differnt state are well written in the chapter 3 of the thesis.
 */
/*---------------------------------------------------------*/

/**
 * State Transition in IDLE_STATE
 * @param data:VCDMsg*. The pointer contains the actual application data.
 */
void VCDClient::IdleProcess(VCDMsg* data)
{
    if (data->getReceiver() == BROADCAST) {
        //BROADCAST Msg;
        LOG_ERROR_BRX;
    } else if (n_num_ != data->getReceiver()) {
        // OVERHEAR Msg;
        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_OVERHEAR_DATA;
                Sharing_timer_.resched(SHARING_TIMEOUT);
                state_ = COVERAGE_STATE;
                break;
            case REQUEST:
                // It receives the header's request msg.
                LOG_OVERHEAR;
                break;
            default:
                LOG_ERROR_OVERHEAR;
                break;
        }
    } else {
        // I am the receiver
        LOG_ERROR_RX;
    }
}

/**
 * State Transition in COVERAGE_STATE 
 * @param data:VCDMsg*. The pointer contains the actual application data.
 */
void VCDClient::CoverageProcess(VCDMsg* data)
{
    if (data->getReceiver() == BROADCAST) {
        //BROADCAST Msg;
        switch ((int) data->getType()) {
            case ADVER:
                LOG_BRX;
                break;
            default:
                LOG_ERROR_BRX;
                break;
        }
    } else if (n_num_ != data->getReceiver()) {
        // OVERHEAR Msg;
        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_OVERHEAR_DATA;
                Sharing_timer_.resched(SHARING_TIMEOUT);
                pkt_ACK_[data->getSeq()] = RECEIVED;
                break;
            default:
                LOG_ERROR_OVERHEAR;
        }
    } else {
        // I am the receiver
        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_RX_DATA;
                state_ = DOWNLOAD_STATE;
                Sharing_timer_.force_cancel();
                Traffic_timer_.resched(TRAFFIC_TIMEOUT);
                pkt_ACK_[data->getSeq()] = RECEIVED;
                break;
            default:
                LOG_ERROR_RX;
        }
    }
}

/**
 * State Transition in DOWNLOAD_STATE
 * @param data:VCDMsg*. The pointer contains the actual application data.
 */
void VCDClient::DownloadProcess(VCDMsg* data)
{
    if (data->getReceiver() == BROADCAST) {
        //broadcast
        switch ((int) data->getType()) {
            case ADVER: {
                LOG_BRX;
                break;
            }

            default:
                LOG_ERROR_BRX;
        }
    } else if (n_num_ != data->getReceiver()) {
        //overhear
        LOG_ERROR_OVERHEAR;
    } else { // I have received the packets that ARE destined to itself
        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_RX_DATA;
                Traffic_timer_.resched(TRAFFIC_TIMEOUT);
                pkt_ACK_[data->getSeq()] = RECEIVED;
                break;
            default:
                LOG_ERROR_RX;
                break;
        }
    }
}

/**
 * State Transition in TRANSFER_WAIT_STATE
 * @param data:VCDMsg*. The pointer contains the actual application data.
 */
void VCDClient::TransferWaitProcess(VCDMsg* data)
{
    if (data->getReceiver() == BROADCAST) {
        //broadcast
        switch ((int) data->getType()) {
            case ADVER: {
                LOG_BRX;
                int flag = 0;

                for (int i = 0; i < FILE_SIZE; i++) {
                    if (pkt_ACK_[i] != RECEIVED && *(data->getPkt() + i) ==
                            RECEIVED) {
                        pkt_I_need_[i] = WANT;
                        flag = 1;
                    } else {
                        pkt_I_need_[i] = UNWANT;
                    }
                }

                if (flag == 0)
                    return;

                transfer_sender_ = data->getSender();

                //TODO two enhancement
                // 1. use power
                // 2. replace the following simple equation.
                double distance =
                    GroupManager::calDistance(transfer_sender_,n_num_);
                double ratio = distance/RADIO_RANGE;

                //TODO
                //0.01 second is to make the timer not to be set to zero.
                //Otherwise the scheduler will not work correctly.
                Cts_tx_timer_.resched(CTS_TX_TIMEOUT*(1-ratio)+0.01);

                Adver_timer_.force_cancel();
                state_ = TRANSFER_RECEIVE_STATE;

                break;
            }

            case FINISH:
                LOG_BRX;
                break;

            default:
                LOG_ERROR_BRX;
                break;
        }
    } else if (n_num_ != data->getReceiver()) {
        // overhear

        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_OVERHEAR_DATA;
                cancelAllTimers();

                Sharing_timer_.resched(SHARING_TIMEOUT);
                pkt_ACK_[data->getSeq()] = RECEIVED;
                state_ = COVERAGE_STATE;
                break;
            case TRANSFER:
                LOG_OVERHEAR_DATA;
                Adver_timer_.sched();
                pkt_ACK_[data->getSeq()] = RECEIVED;
                break;
            default:
                Adver_timer_.sched();
                break;
        }
    } else {
        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_RX_DATA;
                cancelAllTimers();

                pkt_ACK_[data->getSeq()] = RECEIVED;
                Sharing_timer_.resched(SHARING_TIMEOUT);
                state_ = COVERAGE_STATE;
                break;
            default:
                LOG_ERROR_RX;
                break;
        }
    }
}

/**
 * State Transition in CTS_WAIT_STATE
 * @param data:VCDMsg*. The pointer contains the actual application data.
 */
void VCDClient::CtsWaitProcess(VCDMsg* data)
{
    if (data->getReceiver() == BROADCAST) {
        //broadcast
        switch ((int) data->getType()) {
            case ADVER:
                LOG_BRX;
                break;
            default:
                LOG_ERROR_BRX;
                break;
        }
    } else if (n_num_ != data->getReceiver()) {
        //overhear
        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_OVERHEAR_DATA;
                cancelAllTimers();

                Sharing_timer_.resched(SHARING_TIMEOUT);
                pkt_ACK_[data->getSeq()] = RECEIVED;
                state_ = COVERAGE_STATE;
                break;
            case TRANSFER:
                LOG_OVERHEAR_DATA;
                pkt_ACK_[data->getSeq()] = RECEIVED;
                break;
            default:
                LOG_ERROR_OVERHEAR;
                break;
        }
    } else {
        // I am the receiver
        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_RX_DATA;
                cancelAllTimers();
                pkt_ACK_[data->getSeq()] = RECEIVED;
                Sharing_timer_.resched(SHARING_TIMEOUT);
                state_ = COVERAGE_STATE;
                break;
            case CTS:
                LOG_RX;
                state_ = TRANSFER_CTS_STATE;
                updateOtherNeed(data);
                break;
            default:
                LOG_ERROR_RX;
                break;
        }
    }
}

/**
 * State Transition in TRANSFER_CTS_STATE
 * @param data:VCDMsg*. The pointer contains the actual application data.
 */
void VCDClient::TransferCtsProcess(VCDMsg* data)
{
    if (data->getReceiver() == BROADCAST) {
        //broadcast
        LOG_ERROR_BRX;
    } else if (n_num_ != data->getReceiver()) {
        //overhear
        switch ((int) data->getType()) {
            case TRAFFIC:
                LOG_OVERHEAR_DATA;
                cancelAllTimers();

                Sharing_timer_.resched(SHARING_TIMEOUT);
                pkt_ACK_[data->getSeq()] = RECEIVED;
                state_ = COVERAGE_STATE;
                break;
            case TRANSFER:
                LOG_OVERHEAR_DATA;
                pkt_ACK_[data->getSeq()] = RECEIVED;
                break;
            default:
                LOG_ERROR_OVERHEAR;
                break;
        }
    } else {
        // I am the receiver
        switch ((int) data->getType()) {
            case CTS:
                LOG_RX;
                updateOtherNeed(data);
                break;
            case TRAFFIC:
                LOG_RX_DATA;
                cancelAllTimers();

                pkt_ACK_[data->getSeq()] = RECEIVED;
                Sharing_timer_.resched(SHARING_TIMEOUT);
                state_ = COVERAGE_STATE;
                break;
            default:
                LOG_ERROR_RX;
                break;
        }
    }
}

/**
 * State Transition in TRANSFER_RECEIVE_STATE
 * @param data:VCDMsg*. The pointer contains the actual application data.
 */
void VCDClient::TransferReceiveProcess(VCDMsg* data)
{
    if (data->getReceiver() == BROADCAST) {
        // broadcast
        switch ((int) data->getType()) {
            case FINISH:
                LOG_BRX;

                if (transfer_sender_ == data->getSender()) {
                    TransferAck_timer_.force_cancel();
                    Adver_timer_.reset();
                    Adver_timer_.sched();
                    state_ = TRANSFER_WAIT_STATE;
                }
                break;
            case TRAFFIC:
                LOG_BRX;
                cancelAllTimers();

                Sharing_timer_.resched(SHARING_TIMEOUT);
                pkt_ACK_[data->getSeq()] = RECEIVED;
                state_ = COVERAGE_STATE;
                break;
            default:
                LOG_ERROR_BRX;
                break;
        }
    } else if (n_num_ != data->getReceiver()) {
        // overhear

        switch ((int) data->getType()) {
            case TRANSFER:
                LOG_OVERHEAR_DATA;
                pkt_ACK_[data->getSeq()] = RECEIVED;
                TransferAck_timer_.resched(TRANSFER_ACK_TIMEOUT);
                break;
            case TRAFFIC:
                LOG_OVERHEAR_DATA;
                cancelAllTimers();

                pkt_ACK_[data->getSeq()] = RECEIVED;
                Sharing_timer_.resched(SHARING_TIMEOUT);
                state_ = COVERAGE_STATE;
                break;
            default:
                LOG_ERROR_OVERHEAR;
                break;
        }
    } else {
        // I am the receiver
        switch ((int) data->getType()) {
            case TRANSFER:
                LOG_RX_DATA;
                pkt_ACK_[data->getSeq()] = RECEIVED;
                TransferAck_timer_.resched(TRANSFER_ACK_TIMEOUT);
                break;
            case TRAFFIC:
                LOG_RX_DATA;
                cancelAllTimers();

                pkt_ACK_[data->getSeq()] = RECEIVED;
                Sharing_timer_.resched(SHARING_TIMEOUT);
                state_ = COVERAGE_STATE;
                break;
            default:
                LOG_ERROR_RX;
                break;
        }
    }
}

/**
 * State Transition in TRANSFER_SEND_STATE
 * @param data:VCDMsg*. The pointer contains the actual application data.
 */
void VCDClient::TransferSendProcess(VCDMsg* data)
{
    if (data->getReceiver() == BROADCAST) {
        // broadcast
        LOG_ERROR_BRX;
    } else if (n_num_ != data->getReceiver()) {
        //overhear
        switch ((int) data->getType()) {
            case TRANSFER:
                LOG_OVERHEAR_DATA;
                pkt_ACK_[data->getSeq()] = RECEIVED;
                break;
            case TRAFFIC:
                LOG_OVERHEAR_DATA;
                cancelAllTimers();
                pkt_ACK_[data->getSeq()] = RECEIVED;
                Sharing_timer_.resched(SHARING_TIMEOUT);
                state_ = COVERAGE_STATE;
                break;
            default:
                LOG_ERROR_OVERHEAR;
                break;
        }
    } else {
        // I am the receiver
        LOG_ERROR_RX;
    }
}

/**
 * The Client sends the REQUEST message to Server.
 * @param receiver:int. The unique identification of the AP. 
 */
void VCDClient::sendRequest(int receiver)
{
    cout << n_num_ << " sendRequest " << CURRENT_TIME << endl;

    // stop all the possible timers.
    cancelAllTimers();

    Tcl& tcl = Tcl::instance();
    tcl.evalf("$ns_ connect $MessagePassing_(%d) $MessagePassing_(%d)"
              , n_num_, receiver);

    int i;

    for (i = 0; i < FILE_SIZE; i++) {
        if (pkt_ACK_[i] != RECEIVED)
            break;
    }

    VCDMsg *msg = NULL;

    msg = new VCDMsg(REQUEST, n_num_, receiver, i);
    agent_->sendmsgWithAck(msg->size(), msg, this);
    Traffic_timer_.resched(TRAFFIC_TIMEOUT);

    state_ = DOWNLOAD_STATE;
}

/**
 * The Client receives the CTS message and updates the pkt_other_need_ list.
 * It records both the packet number and the id of the vehicle which needs the packet. 
 * @param data:VCDMsg*. The application data which contains CTS message. 
 */
void VCDClient::updateOtherNeed(VCDMsg* data)
{
    Pkt_State_t* pkt = data->getPkt();
    int id = data->getSender();

    for (int i = 0; i < FILE_SIZE; i++) {
        if (pkt[i] == WANT) {
            pkt_other_need_[i].id_ = id;
            pkt_other_need_[i].status_ = WANT;
        }
    }
}

/**
 * The Client is in TRANSFER_WAIT_STATE. After AdverTimer timeout, it sends the broadcast ADVER message.
 */
void VCDClient::sendAdver()
{
    LOG_BTX("ADVER ");

    VCDMsg *msg = NULL;

    msg = new VCDMsg(ADVER, n_num_, pkt_ACK_);
    ns_addr_t dst;
    dst.addr_ = IP_BROADCAST;
    dst.port_ = 0;

    agent_->sendto(msg->size(), msg, NULL, dst);
}

/**
 * The Client is in TRANSFER_WAIT_STATE and receives the ADVER message. Now it sends back the CTS message.
 * The receiver of CTS is transfer_sender_, which is already assigned in the state transition code.
 */
void VCDClient::sendCTS()
{
    Tcl& tcl = Tcl::instance();
    tcl.evalf("$ns_ connect $MessagePassing_(%d) $MessagePassing_(%d)", n_num_,
              transfer_sender_);

    VCDMsg *msg = NULL;
    msg = new VCDMsg(CTS, n_num_, transfer_sender_, pkt_I_need_);

    agent_->sendmsgWithAck(msg->size(), msg, this);

    LOG_TX_CTS;
}

/**
 * The Client is in the TRANSFER_SEND_STATE and sends the TRANSFER message.
 */
void VCDClient::transferTx()
{
    // a simple way to find the first available packet
    while (pkt_other_need_[transfer_No].status_ != WANT) {
        transfer_No++;

        if (transfer_No >= FILE_SIZE) {
#ifdef DEBUG_VCD
            cout << "No." << n_num_ << " Transfer End " << CURRENT_TIME << endl;
#endif
        cout << "No." << n_num_ << " Transfer End " << CURRENT_TIME << endl;

            state_ = TRANSFER_WAIT_STATE; // all transferred.
            Adver_timer_.reset();
            Adver_timer_.sched();
            VCDMsg *msg = NULL;

            msg = new VCDMsg(FINISH, n_num_);
            ns_addr_t dst;
            dst.addr_ = IP_BROADCAST;
            dst.port_ = 0;

            agent_->sendto(msg->size(), msg, NULL, dst);
            return;
        }
    }

    VCDMsg *msg = NULL;

    msg = new VCDMsg(TRANSFER, n_num_,
                     pkt_other_need_[transfer_No].id_,
                     PACKET_SIZE, transfer_No);

    LOG_PKT_TX(transfer_No, pkt_other_need_[transfer_No].id_);

    Tcl& tcl = Tcl::instance();
    tcl.evalf("$ns_ connect $MessagePassing_(%d) $MessagePassing_(%d)",
              n_num_, pkt_other_need_[transfer_No].id_);

    agent_->sendmsgWithAck(msg->size(), msg, this);
}

/*-----------------------------------------------------*/
/**
 * Main Timeout Structure
 * Do something according to the current state
 * The following are the timeout handling functions.
 */
/*-----------------------------------------------------*/
/**
 * TrafficTimer Timeout Handling Functions
 */
void VCDClient::TrafficTimeout()
{
    switch (state_) {
        case DOWNLOAD_STATE:
            LOG_TIMEOUT(" TrafficTimer ");
            state_ = COVERAGE_STATE;
            break;
        default:
            LOG_ERROR_TIMEOUT(" TrafficTimer ");
    }

    Sharing_timer_.resched(SHARING_TIMEOUT);
}

/**
 * AdverTimer Timeout Handling Functions
 */
void VCDClient::AdverTimeout()
{
    switch (state_) {
        case TRANSFER_WAIT_STATE:
            LOG_TIMEOUT(" AdverTimer ");
            state_ = CTS_WAIT_STATE;
            sendAdver();
            CTS_timer_.resched(CTS_TIMEOUT);
            break;
        default:
            LOG_ERROR_TIMEOUT(" AdverTimer ");
    }
}

/**
 * CTSTimer Timeout Handling Functions
 */
void VCDClient::CTSTimeout()
{
    switch (state_) {
        case CTS_WAIT_STATE:
            LOG_TIMEOUT(" CTSTimer ");
            state_ = TRANSFER_WAIT_STATE;
            Adver_timer_.txMore();
            Adver_timer_.sched();
            break;
        case TRANSFER_CTS_STATE:
            LOG_TIMEOUT(" CTSTimer ");
            state_ = TRANSFER_SEND_STATE;
            transfer_No = 0;
            transferTx();
            break;
        default:
            LOG_ERROR_TIMEOUT(" CTSTimer ");
    }

}

/**
 * TransferTimer Timeout Handling Functions
 */
void VCDClient::TransferTimeout()
{
    switch (state_) {
        case COVERAGE_STATE:
            LOG_TIMEOUT(" SharingTimer ");
            state_ = TRANSFER_WAIT_STATE;
            break;
        default:
            LOG_ERROR_TIMEOUT(" SharingTimer ");
    }

    Adver_timer_.reset();

    Adver_timer_.sched();
}

/**
 * TransferAckTimer Timeout Handling Functions
 */
void VCDClient::TransferAckTimeout()
{
    switch (state_) {
        case TRANSFER_RECEIVE_STATE:
            LOG_TIMEOUT(" TransferAckTimer ");
            Adver_timer_.reset();
            Adver_timer_.sched();
            state_ = TRANSFER_WAIT_STATE;
            break;
        default:
            LOG_ERROR_TIMEOUT(" TransferAckTimer ");
    }
}

/**
 * CtsTxTimer Timeout Handling Functions
 */
void VCDClient::CtsTxTimeout()
{
    switch (state_) {
        case TRANSFER_RECEIVE_STATE:
            LOG_TIMEOUT(" CtsTxTimeout ");
            sendCTS(); //Tx CTS
            Adver_timer_.force_cancel(); //stop Adver Timer
            TransferAck_timer_.resched(TRANSFER_ACK_TIMEOUT);
            break;
        default:
            LOG_ERROR_TIMEOUT(" CtsTxTimeout ");
    }
}

/**
 * The Client successfully transmits the packet.
 * It is called when 802.11 MAC layer sender correctly receives the ACK message.
 * In order to make this work, the agent should use the sendmsgWithAck function: "agent_->sendmsgWithAck(msg->size(), msg, this);"
 */
void VCDClient::correctSend()
{
    switch (state_) {
        case TRANSFER_SEND_STATE:
            pkt_other_need_[transfer_No].status_ = UNWANT;
            transferTx();
        default:
            break;
    }
}

/**
 * The Client fails to transmit the packet.
 * It is called when 802.11 MAC layer sender still fails to receive the ACK message when the retransmission times is larger than the threshold.
 * In order to make this work, the agent should use the sendmsgWithAck function: "agent_->sendmsgWithAck(msg->size(), msg, this);"
 */
void VCDClient::inCorrectSend()
{
    switch (state_) {
        case TRANSFER_SEND_STATE:
            pkt_other_need_[transfer_No].status_ = UNWANT;
            transferTx();
        default:
            break;
    }
}

/**
 * Check whether the Client has received all the packets or not.
 * @return int. VALID if it has reveived all the packets. INVALID if it has not. 
 */
int VCDClient::checkFinish()
{
    for (int i = 0; i < FILE_SIZE; i++) {
        if (pkt_ACK_[i] != RECEIVED) {
            return INVALID;
        }
    }

    return VALID;
}

/*----------------------------------------------------------*/
/**
 * The following six functions are the setters and getters of the three private variables.
 */
/*----------------------------------------------------------*/
void VCDClient::updateState(VCD_State_t state)
{
    state_ = state;
}

VCD_State_t VCDClient::getState()
{
    return state_;
}

void VCDClient::updateAttachedAP(int AP)
{
    attached_AP_ = AP;
}

int VCDClient::getAttachedAP()
{
    return attached_AP_;
}

void VCDClient::setTransferSender(int sender)
{
    transfer_sender_ = sender;
}

int VCDClient::getTransferSender()
{
    return transfer_sender_;
}

/**
 * Cancel all the six timers in the VCDClient 
 */
void VCDClient::cancelAllTimers()
{
    Adver_timer_.force_cancel();
    CTS_timer_.force_cancel();
    Traffic_timer_.force_cancel();
    Sharing_timer_.force_cancel();
    TransferAck_timer_.force_cancel();
    Cts_tx_timer_.force_cancel();
}

/*--------------------------------------------------------------------------- */
/*--------------------------VCDServer---------------------------------------- */
/*--------------------------------------------------------------------------- */

/**
 * VCDAppServerClass is used to create VCDClient in C++ domain when the user invokes the command in the OTcl domain. 
 * It is necessary when we have to add a new application.
 */
static class VCDAppServerClass : public TclClass
{
public:
    VCDAppServerClass() : TclClass("Application/VCDServer") {}

    TclObject* create(int, const char*const*) {
        return (new VCDServer);
    }
} class_app_VCDServer;

/**
 * The constructor of VCDServer. 
 * It binds the n_num_ variable with the id_ variable in OTcl domain. 
 *
 * It initialzes state and seqno. 
 */
VCDServer::VCDServer()
{
    state_ = IDLE_STATE;
    seqno_ = 0;
    bind("id_", &n_num_);
}

/**
 * The function is invoked in the first place when the simulation begins. 
 * The server is static. So the location does not need to be updated by GroupManager. We only have to update once here.  
 */
void VCDServer::start()
{
    //cout << "VCDServer No." << n_num_ << " starts!" << endl;
    // the server is static, I do not use GPS update timer to update it.
    MobileNode* n = (MobileNode*)
                    Node::get_node_by_address(agent_->addr());
    GroupManager::updateLoc(n, n_num_);
}

/**
 * Although VCDServer does not receive any specific command from the OTcl domain, we have to override this function.
 * It calls the Application::command, which accepts common command for Application.
 */
int VCDServer::command(int argc, const char*const* argv)
{
    return (Application::command(argc, argv));
};

/**
 * The function is invoked when the simulation ends.
 * It counts the number of packets that the client receives and the current state of the Server.
 */
void VCDServer::stop()
{
    cout << "---- VCDServer ---- " << endl;
    cout << "Server transfered " << seqno_ + 1 << " pkts." << endl << endl;
    // because the seqno_ is counted from zero, the number of
    // the sent packets should be added one.
}

/**
 * The Server successfully transmits the packet.
 * It is called when 802.11 MAC layer sender correctly receives the ACK message.
 * In order to make this work, the agent should use the sendmsgWithAck function: "agent_->sendmsgWithAck(msg->size(), msg, this);"
 * Then it transmits the next packet to the header again. 
 */
void VCDServer::correctSend()
{
    //cout << "VCDServer correctly sends something!" << endl;

    if (state_ != DOWNLOAD_STATE) {
        cout << "Node No." << n_num_ << " State Error " << CURRENT_TIME << endl;
        return;
    }

    seqno_++;

    // all packets have been sent

    if (seqno_ >= FILE_SIZE) {
        state_ = FINISH_STATE;
        cout << "all pkt finished at " << CURRENT_TIME << endl;
        return;
    }

    LOG_PKT_TX(seqno_+1, sending_n_num_);

    VCDMsg* msg = new VCDMsg(TRAFFIC, n_num_, sending_n_num_,
                             PACKET_SIZE,
                             seqno_ + 1);

    Tcl& tcl = Tcl::instance();
    tcl.evalf("$ns_ connect $MessagePassing_(%d) $MessagePassing_(%d)",
              n_num_, sending_n_num_);

    agent_->sendmsgWithAck(msg->size(), msg, this);

}

/**
 * The Client fails to transmit the packet.
 * It is called when 802.11 MAC layer sender still fails to receive the ACK message when the retransmission times is larger than the threshold.
 * In order to make this work, the agent should use the sendmsgWithAck function: "agent_->sendmsgWithAck(msg->size(), msg, this);"
 * It finds the next header in the group. Then it transmits the packet again to the new header. If no header is available, the Server stops sending the packets.
 */
void VCDServer::inCorrectSend()
{
#ifdef DEBUG_VCD
    cout << "No." << n_num_ << " incorrectly Tx! " << CURRENT_TIME << endl;
#endif

    if (state_ != DOWNLOAD_STATE) {
        cout << "Node No." << n_num_ << " State Error " << CURRENT_TIME << endl;
        return;
    }

    sending_n_num_ = INVALID;

    double min_distance = (double)MAXIMUM;

    double distance;

    for (int j = 0; j < VEHICLE_NUM; j++) {

        distance = GroupManager::calDistance(j, n_num_);

        if (distance < min_distance) {
            min_distance = distance;
            sending_n_num_ = j;
        }
    }

    // no more candidate available
    if (min_distance > RADIO_RANGE) {
        state_ = IDLE_STATE;
        sending_n_num_ = INVALID;
        return;
    }

#ifdef DEBUG_VCD
    cout << "---change to " << sending_n_num_ << " " << CURRENT_TIME << endl;
#endif

    LOG_PKT_TX(seqno_+1, sending_n_num_);

    // build the message & send it
    VCDMsg* msg = new VCDMsg(TRAFFIC, n_num_, sending_n_num_,
                             PACKET_SIZE,
                             seqno_ + 1);

    Tcl& tcl = Tcl::instance();

    tcl.evalf("$ns_ connect $MessagePassing_(%d) $MessagePassing_(%d)",
              n_num_, sending_n_num_);

    agent_->sendmsgWithAck(msg->size(), msg, this);
}

/**
 * It is the main packet receive function. It is called by the lower transport agent. In our implementation, it is the MessagePassing Agent.
 * Different function is called depending on the current state.
 * The Server only accepts REQEUST message when it is in IDLE_STATE
 *
 * @param recvData:AppData. The application data is stored in this pointer
 * @param size:int. The size of the application data.
 * 
 */
void VCDServer::process_data(int size, AppData* recvData)
{
    VCDMsg* data = (VCDMsg *) recvData;

    switch (state_) {
        case IDLE_STATE:
            IdleProcess(data);
            break;
        case DOWNLOAD_STATE:
            break;
        case FINISH_STATE:
            cout << "all data in the server have been sent out" << endl;
            return;
            break;
        default:
            break;
    }
}

/**
 * State Transition in IDLE_STATE
 * @param data:VCDMsg*. The pointer contains the actual application data.
 * It receives the REQUEST message and transmits back the TRAFFIC message.
 */
void VCDServer::IdleProcess(VCDMsg* data)
{

    if (data->getReceiver() == BROADCAST) {
        // broadcast
        LOG_ERROR_BRX;
    } else if (n_num_ != data->getReceiver()) {
        //overhear
        LOG_ERROR_OVERHEAR;
    } else {
        switch (data->getType()) {
                // I am the receiver
            case REQUEST: {
                LOG_RX;

                sending_n_num_ = data->getSender();
                seqno_ = data->getSeq() - 1;

                LOG_PKT_TX(seqno_+1, sending_n_num_);

                VCDMsg* msg = new VCDMsg(TRAFFIC, n_num_, data->getSender(),
                                         PACKET_SIZE, seqno_ + 1);

                Tcl& tcl = Tcl::instance();
                tcl.evalf(
                    "$ns_ connect $MessagePassing_(%d) $MessagePassing_(%d)",
                    n_num_, data->getSender());
                agent_->sendmsgWithAck(msg->size(), msg, this);
                state_ = DOWNLOAD_STATE;

                break;
            }

            default:
                LOG_ERROR_RX;
        }
    }

}

/*--------------------------------------------------------------------------- */
/*--------------------------GroupManager------------------------------------- */
/*--------------------------------------------------------------------------- */

/**
 * GroupManagerClass is used to create GroupManager in C++ domain when the user invokes the command in the OTcl domain. 
 */
static class GroupManagerClass : public TclClass
{
public:
    GroupManagerClass() : TclClass("GroupManager") {}

    TclObject* create(int, const char*const*) {
        return (new GroupManager);
    }
} class_Group_Manager;

/**
 * The constructor of GroupManager. 
 * It allocates the memory space for the lists and assigns initial values.
 * There is only one timer: GPSUpdateTimer
 */
GroupManager::GroupManager() : update_timer_(this)
{
    vcd_list_ = (VCDClient**)malloc(sizeof(VCDClient*) * VEHICLE_NUM);
    node_list_ = (MobileNode**)malloc(sizeof(MobileNode*) * VEHICLE_NUM);
    group_header_ = (int*)malloc(sizeof(int) * (AP_NUM));
    pos_info_ = (Node_Info_t*)malloc(sizeof(Node_Info_t) *
                                     (AP_NUM +
                                      VEHICLE_NUM));

    // initialization
    memset(vcd_list_, 0, sizeof(VCDClient*) *(VEHICLE_NUM));
    memset(node_list_, 0, sizeof(MobileNode*) *(VEHICLE_NUM));
    memset(group_header_, INVALID, sizeof(int) *(AP_NUM));

    for (int i = 0; i < VEHICLE_NUM + AP_NUM; i++) {
        pos_info_[i].id_ = i;
    }

    //initialization seeds
    srand(time(NULL));
}

/**
 * It is the destructor of the GroupManager.
 * It frees the memory spaces for the lists.
 */
GroupManager::~GroupManager()
{
    free(vcd_list_);
    free(node_list_);
    free(group_header_);
    free(pos_info_);
}

/**
 * It receives commands from OTcl domain.
 * The function only accepts "start" command. It calls the start() function.
 * @param argc:integer. The number of command line arguments.
 * @param argv:char**. The full list of all of the command line arguments.
 * @param int. -1, if the function can not recognize the command.
 *             TCL_OK, if the command is successfully executed.
 */
int GroupManager::command(int argc, const char*const* argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "start") == 0) {
            start();
            return(TCL_OK);
        }
    }

    return -1;
}

/**
 * It starts the GPS update timer at GPS_INTERVAL time.
 */
void GroupManager::start()
{
    cout << "group manager starts" << endl;
    update_timer_.resched(GPS_INTERVAL);
}

/**
 * It is called when the VCDGPSUpdateTimer reaches timeout value.
 * It firstly updates the location of the vehicle. 
 * Then it calculates its distance from an AP. 
 * If it is inside one AP's coverage range and is the header of the group at that moment, it transmits REQUEST message. 
 * If it is inside one AP's coverage range but is not the header, it only updates the attached_ap vaiable in the class. 
 * If it is outside all AP's coverage range, it clears the attached_ap variable. 
 */
void GroupManager::GPSUpdate()
{
    // update  all the positions
    double distance = -1;

    for (int j = 0; j < VEHICLE_NUM; j++) {

        GroupManager::updateLoc(node_list_[j], j);

        // determine whether it is inside the range

        for (int i = VEHICLE_NUM; i < VEHICLE_NUM + AP_NUM; i++) {

            distance = GroupManager::calDistance(i, j);

            // It is inside the range of one AP

            if (distance < RADIO_RANGE) {
                vcd_list_[j]->updateAttachedAP(i);

                if (group_header_[i-VEHICLE_NUM] == INVALID) {
                    // no header is assigned yet
                    group_header_[i-VEHICLE_NUM] = j;
                    // ask the nodes to send request
                    Tcl& tcl = Tcl::instance();
                    tcl.evalf("$VCD_(%d) sendRequest %d", j, i);
                }
            }

            // It moves out from the AP's range
            if (distance > RADIO_RANGE) {
                vcd_list_[j]->updateAttachedAP(INVALID);
            }
        }
    }

    update_timer_.resched(GPS_INTERVAL);
}

/**
 * Update the location of the nodes.
 * @param num:int. The identification of the node.
 * @param n:MobilNode*. The internal pointer to the node, from which we can get the position.
 */
void GroupManager::updateLoc(MobileNode *n, int num)
{
    double i;
    n->getLoc(&pos_info_[num].x_, &pos_info_[num].y_, &i);
}

/**
 * Calculate the distance between node i and node j.
 */
double GroupManager::calDistance(int i, int j)
{
    return sqrt((pos_info_[j].x_ -pos_info_[i].x_)*(pos_info_[j].x_
                -pos_info_[i].x_) + (pos_info_[j].y_
                                     -pos_info_[i].y_)*(pos_info_[j].y_
                                                        -pos_info_[i].y_));
}

/*--------------------------------------------------------------------------- */
/*--------------------------Timers------------------------------------------- */
/*--------------------------------------------------------------------------- */

/*------------------------------------------------*/
/**
 * This comment holds for the next seven timers.
 * The pointer to the application is assigned when the timer is created. 
 * Then the corresponding void expire(Event *) function is called by the simulator when the timer reaches the timeout time. 
 * So inside the expire function, we call the corresponding function to handle the timeout event.
 */
/*------------------------------------------------*/
void VCDGPSUpdateTimer::expire(Event *)
{
    manager_->GPSUpdate();
    return;
}

void VCDClientTrafficTimer::expire(Event *)
{
    app_->TrafficTimeout();
    return;
}

void VCDClientSharingTimer::expire(Event *)
{
    app_->TransferTimeout();
    return;
}

void VCDClientAdverTimer::expire(Event *)
{
    app_->AdverTimeout();
    return;
}

void VCDClientCTSTimer::expire(Event *)
{
    app_->CTSTimeout();
    return;
}

void VCDClientTransferAckTimer::expire(Event *)
{
    app_->TransferAckTimeout();
    return;
}

void VCDClientCtsTxTimer::expire(Event *)
{
    app_->CtsTxTimeout();
    return;
}

/**
 * Reset the retransmission times to one.
 */
void VCDClientAdverTimer::reset()
{
    re_tx_ = 1;
}

/**
 * Increase the retransmission times by one.
 */
void VCDClientAdverTimer::txMore()
{
    re_tx_++;
}

/**
 * Schedule sending time randomly between 0 and the maximum timeout value. 
 * The maximum time value is the ADVER_TIMEOUT multiplying the retransmission times.
 */ 
void VCDClientAdverTimer::sched()
{
    // random number from 0.01 to 1
    double num = (double)(rand() % 100 + 1)/100;
    resched(ADVER_TIMEOUT*((double)(re_tx_ * (1 - num))));
}

/*--------------------------------------------------------------------------- */
/*--------------------------InputParameter----------------------------------- */
/*--------------------------------------------------------------------------- */

/**
 * InputParameterClass is used to create InputParameter C++ domain when the user invokes the command in the OTcl domain. 
 */
static class InputParameterClass : public TclClass
{
public:
    InputParameterClass() : TclClass("InputParameter") {}

    TclObject* create(int, const char*const*) {
        return (new InputParameter);
    }

    virtual void bind();
    virtual int method(int argc, const char*const* argv);

} class_Input_Parameter;

/**
 * The parameters are defined as the static member variables of the class such that other class can access these variables without referring to one specific instance of the class.    
 * The mechanisms about how to bind static C++ class member variables is provided in the manual
 * Create the binding methods in the implementation of bind() with add_method()
 */
void InputParameterClass::bind()
{
    TclClass::bind();
    add_method("gps_interval_");
    add_method("adver_timeout_");
    add_method("traffic_timeout_");
    add_method("sharing_timeout_");
    add_method("cts_timeout_");
    add_method("transfer_ack_timeout_");
    add_method("cts_tx_timeout_");
    add_method("packet_size_");
    add_method("file_size_");
    add_method("ap_num_");
    add_method("vehicle_num_");
    add_method("radio_range_");
}

/**
 * Implement the handler in method() in a similar way as one would do in TclObject::command(). 
 * Notice that the number of arguments passed to TclClass::method() are different from those passed to TclObject::command(). 
 * The former has two more arguments in the front.
 */
int InputParameterClass::method(int ac, const char*const* av)
{
    Tcl& tcl = Tcl::instance();
    int argc = ac - 2;
    const char*const* argv = av + 2;

    if (argc == 2) {
        if (strcmp(argv[1], "gps_interval_") == 0) {
            tcl.resultf("%f", InputParameter::gps_interval_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "adver_timeout_") == 0) {
            tcl.resultf("%f", InputParameter::adver_timeout_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "traffic_timeout_") == 0) {
            tcl.resultf("%f", InputParameter::traffic_timeout_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "sharing_timeout_") == 0) {
            tcl.resultf("%f", InputParameter::sharing_timeout_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "cts_timeout_") == 0) {
            tcl.resultf("%f", InputParameter::cts_timeout_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "transfer_ack_timeout_") == 0) {
            tcl.resultf("%f", InputParameter::transfer_ack_timeout_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "cts_tx_timeout_") == 0) {
            tcl.resultf("%f", InputParameter::cts_tx_timeout_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "packet_size_") == 0) {
            tcl.resultf("%d", InputParameter::packet_size_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "file_size_") == 0) {
            tcl.resultf("%d", InputParameter::file_size_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "ap_num_") == 0) {
            tcl.resultf("%d", InputParameter::ap_num_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "vehicle_num_") == 0) {
            tcl.resultf("%d", InputParameter::vehicle_num_);
            return (TCL_OK);
        } else if (strcmp(argv[1], "radio_range_") == 0) {
            tcl.resultf("%f", InputParameter::radio_range_);
            return (TCL_OK);
        } else {
            return (TCL_ERROR);
        }

    } else if (argc == 3) {
        if (strcmp(argv[1], "gps_interval_") == 0) {
            InputParameter::gps_interval_ = atof(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "adver_timeout_") == 0) {
            InputParameter::adver_timeout_ = atof(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "traffic_timeout_") == 0) {
            InputParameter::traffic_timeout_ = atof(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "sharing_timeout_") == 0) {
            InputParameter::sharing_timeout_ = atof(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "cts_timeout_") == 0) {
            InputParameter::cts_timeout_ = atof(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "transfer_ack_timeout_") == 0) {
            InputParameter::transfer_ack_timeout_ = atof(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "cts_tx_timeout_") == 0) {
            InputParameter::cts_tx_timeout_ = atof(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "packet_size_") == 0) {
            InputParameter::packet_size_ = atoi(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "file_size_") == 0) {
            InputParameter::file_size_ = atoi(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "ap_num_") == 0) {
            InputParameter::ap_num_ = atoi(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "vehicle_num_") == 0) {
            InputParameter::vehicle_num_ = atoi(argv[2]);
            return (TCL_OK);
        } else if (strcmp(argv[1], "radio_range_") == 0) {
            InputParameter::radio_range_ = atof(argv[2]);
            return (TCL_OK);
        } else {
            return (TCL_ERROR);
        }
    }

    return TclClass::method(ac, av);
}

InputParameter::InputParameter()
{
}

/*--------------------------------------------------------------------------- */
/*--------------------------VCDMsg------------------------------------------- */
/*--------------------------------------------------------------------------- */
/**
 * Return the packet status field size using Run Length Coding.
 * @return the size of the field.
 */ 
int VCDMsg::pktStatusSize()
{
    Pkt_State_t previous = pkt_[0];
    int size = 1 + sizeof(int);

    for (int i = 1; i < FILE_SIZE; i++) {
        if (pkt_[i] != previous) {
            size += sizeof(int);
            previous = pkt_[i];
        }
    }

    return size;
}
