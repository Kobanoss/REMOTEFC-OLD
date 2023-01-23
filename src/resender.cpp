#include "resender.h"
#include <QCoreApplication>
#include <QDebug>
//-----------------------------------------------------------------------
#define SERVER_IP "89.108.114.104"
#define SERVER_PORT 50000
//-----------------------------------------------------------------------
#define MSG_SIZE 1445
//-----------------------------------------------------------------------
#define MSG_ANY_ERROR_VERIFICATION_TO_DATA "BAD_VERIFICATION_CODE"
#define MSG_ANY_ERROR_VERIFICATION_TO_SIZE 21

#define MSG_ANY_CONNECTED_TO_DATA "VERIFIED"
#define MSG_ANY_CONNECTED_TO_SIZE 8

#define MSG_ANY_DISCONNECT_FROM_DATA  QString("### DisconnectObject ###")
//-----------------------------------------------------------------------
#define MSG_ESP_ENTRY_FROM_DATA QString("ESP_ENTRY")

#define MSG_ESP_STATUS_REQUEST_TO_DATA "FB_REQUEST" 
#define MSG_ESP_STATUS_REQUEST_TO_SIZE 10
#define MSG_ESP_STATUS_REQUEST_FROM_DATA QString("ON_LINE")
//-----------------------------------------------------------------------
#define MSG_ESP_METRICS_REQUEST_TO_DATA "!DATA"
#define MSG_ESP_METRICS_REQUEST_TO_SIZE 5
#define MSG_ESP_METRICS_REQUEST_FROM_STARTSWITH QString("!DATA")
//-----------------------------------------------------------------------
#define MSG_CLIENT_ENTRY_DATA QString("CLIENT_ENTRY")

#define MSG_CLIENT_ERROR_NO_ESP_DATA "ESP not connected!"
#define MSG_CLIENT_ERROR_NO_ESP_SIZE 18

#define MSG_CLIENT_PING_FROM_DATA QString("!PING")
#define MSG_CLIENT_PING_TO_DATA "!PONG"
#define MSG_CLIENT_PING_TO_SIZE 5
//-----------------------------------------------------------------------
#define TIMER_REQUEST_TO_DELAY 2000
#define TIMER_REQUEST_FROM_DELAY 7000
#define TIMER_METRICS_TO_DELAY 8000
//-----------------------------------------------------------------------
#define W_url QString("api.openweathermap.org")
#define W_path QString("/data/2.5/onecall?")
#define W_cords QString("lat=55.7522&lon=37.6156")
#define W_datatype QString("&exclude=minutely,hourly,daily,alerts")
#define W_apicode QString("&appid=caefecd464018c404a100e82292c89a0")
//-----------------------------------------------------------------------
#define LOG_FILE QString("./log.txt")
//-----------------------------------------------------------------------

UdpResender::UdpResender(QObject *parent) : QObject(parent) {
    
    qDebug() << "Server IP:   " << SERVER_IP;
    qDebug() << "Server PORT: " << SERVER_PORT;
    qDebug() << "";
    
    UdpSocket = new QUdpSocket(this);

    UdpSocket->bind(SERVER_PORT,QUdpSocket::ShareAddress);
    connect(UdpSocket, &QUdpSocket::readyRead, this, &UdpResender::readPendingDatagrams, Qt::UniqueConnection);

    timer_req_send = new QTimer(this);
    timer_req_ans = new QTimer(this);
    timer_req_metrics = new QTimer(this);

    connect(timer_req_ans, &QTimer::timeout, this, &UdpResender::removeESPData);
    connect(timer_req_send, &QTimer::timeout, this, &UdpResender::sendStatusRequest);
    connect(timer_req_metrics, &QTimer::timeout, this, &UdpResender::sendMetricsRequest);
}
 

UdpResender::~UdpResender() {

    UdpSocket->close();
}


void UdpResender:: removeESPData() {
        timer_req_send->stop();
        timer_req_ans->stop();
        timer_req_metrics->stop();

        esp_data.ip =  QHostAddress("0.0.0.0");
        esp_data.port = 0;
        esp_data.status = Disconnected;
        esp_data.verification_code = "";

        qDebug() << "ESP DISCONNECTED";
        qDebug() << "";
        logEspMessage("DISCONNECTED!");
}


void UdpResender::removeClientData(QString hash_code) {
    clients_data.remove(hash_code);
}

void UdpResender::sendStatusRequest() {
    logEspMessage("SEND REQUEST!");
    UdpSocket->writeDatagram(MSG_ESP_STATUS_REQUEST_TO_DATA, MSG_ESP_STATUS_REQUEST_TO_SIZE, esp_data.ip, esp_data.port);
    if (!timer_req_ans->isActive())
        timer_req_ans->start(TIMER_REQUEST_FROM_DELAY);
}

void UdpResender::sendMetricsRequest() {
    UdpSocket->writeDatagram(MSG_ESP_METRICS_REQUEST_TO_DATA, MSG_ESP_METRICS_REQUEST_TO_SIZE, esp_data.ip, esp_data.port);
}

void UdpResender::logEspMessage(QString data) {
    QFile file(LOG_FILE);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return;

    QTextStream out(&file);
    out << QDateTime().currentDateTime().toString() << '\n' << data << '\n';
    file.close();
}


bool UdpResender::isAnyClientsConnected() {
    if (clients_data.isEmpty())
        return false;
    for (auto &&key: clients_data.keys()) {
        if (clients_data[key].status == Connected)
            return true;
    }
    return false;
}


UserType UdpResender::getUserType(QHostAddress sender_ip, quint16 sender_port) {
    if (sender_ip == esp_data.ip && sender_port == esp_data.port)
        return ESP;
    if (clients_data.contains(generateUserHash(sender_ip, sender_port)))
        return CLIENT;
    return OTHER_TARGET;
}

QString UdpResender::generateVerificationCode(QHostAddress sender_ip) {
    
    // TODO: MODIFY
    qDebug() << "Generated code for <" << sender_ip.toString() << ">: <1234>\n";
    return QString::number(1234);
}

void UdpResender::customMulticast(QByteArray msg) {
    for (auto &&key: clients_data.keys())
        if (clients_data[key].status == Connected)
            UdpSocket->writeDatagram(msg.data(), msg.size(), clients_data[key].ip, clients_data[key].port);
}


QString UdpResender::generateUserHash(QHostAddress sender_ip, quint16 sender_port) {
    return QCryptographicHash::hash((sender_ip.toString() + QString::number(sender_port)).toUtf8(),QCryptographicHash::Md5).toHex();
}


void UdpResender::disconnectUser(QHostAddress sender_ip, quint16 sender_port, UserType target) {
    if (target == ESP) {
        removeESPData();   

        qDebug() << "Requested ESP Disconnect";
        qDebug() << "ESP IPv6: " << sender_ip.toString();
        qDebug() << "ESP IPv4: " << sender_ip.toString().mid(7);
        qDebug() << "ESP PORT: " << sender_port;
        qDebug() << "ESP DISCONNECTED";
    }
    else if (target == CLIENT) {
        QString hash = generateUserHash(sender_ip, sender_port);
        removeClientData(hash);  

        qDebug() << "Requested CLIENT Disconnect\n";
        qDebug() << "CLIENT IPv6: " << sender_ip.toString();
        qDebug() << "CLIENT IPv4: " << sender_ip.toString().mid(7);
        qDebug() << "CLIENT PORT: " << sender_port;
        qDebug() << "CLIENT Disconnected";
    }

    else {
        qDebug() << "Troubles in disconnection process!";
        qDebug() << "NON CLIENT/ESP target requested disconnect!";
        qDebug() << "TARGET IPv6: " << sender_ip.toString();
        qDebug() << "TARGET IPv4: " << sender_ip.toString().mid(7);
        qDebug() << "TARGET PORT: " << sender_port;    

        UdpSocket->writeDatagram("Can't disconnect non-esp/client object", 38, sender_ip, sender_port);
    }

    qDebug() << "";
}


void UdpResender::startVerificationProcess(QHostAddress sender_ip, quint16 sender_port) {

     if (datagram.data() == MSG_ESP_ENTRY_FROM_DATA) {

        qDebug() << "Requested ESP Entry\n";
        qDebug() << "ESP IPv6: " << sender_ip.toString();
        qDebug() << "ESP IPv4: " << sender_ip.toString().mid(7);
        qDebug() << "ESP PORT: " << sender_port;
        qDebug() << "Send verification key to ESP...";
        qDebug() << "";

        esp_data.ip = sender_ip;
        esp_data.port = sender_port;
        esp_data.status = Started;

        auto code = generateVerificationCode(sender_ip);
        // TODO: MODIFY 
        esp_data.verification_code = code;
        // TODO: MODIFY 

        QString code_str = code;

        UdpSocket->writeDatagram(code_str.toStdString().c_str(), code_str.length(), sender_ip, sender_port);
    }
    else if (datagram.data() == MSG_CLIENT_ENTRY_DATA) {
        QString hash = generateUserHash(sender_ip, sender_port);
        
        auto code = generateVerificationCode(sender_ip);

        clients_data[hash].ip = sender_ip;
        clients_data[hash].port = sender_port;
        clients_data[hash].status = Started;
        clients_data[hash].verification_code = code;

        QString code_str = code; 

        qDebug() << "Requested CLIENT Entry\n";
        qDebug() << "CLIENT IPv6: " << sender_ip.toString();
        qDebug() << "CLIENT IPv4: " << sender_ip.toString().mid(7);
        qDebug() << "CLIENT PORT: " << sender_port;
        qDebug() << "Send verification key to CLIENT...";
        qDebug() << "";


        UdpSocket->writeDatagram(code_str.toStdString().c_str(), code_str.length(), sender_ip, sender_port);
    }
    else {
        qDebug() << "Trouble on the start of verification!";
        qDebug() << "";
        UdpSocket->writeDatagram("Can't connect non-esp/client object", 35, sender_ip, sender_port);
    }
}


void UdpResender::endVerificationProcess(QHostAddress sender_ip, quint16 sender_port, UserType target) {
    if (target == ESP) {
        if (datagram.data() == esp_data.verification_code) {
            esp_data.status = Connected;
            qDebug() << "ESP Entry VERIFIED\n";
            logEspMessage("ESP CONNECTED!");
            UdpSocket->writeDatagram(MSG_ANY_CONNECTED_TO_DATA, MSG_ANY_CONNECTED_TO_SIZE, sender_ip, sender_port);
            timer_req_send->start(TIMER_REQUEST_TO_DELAY);
            timer_req_metrics->start(TIMER_METRICS_TO_DELAY);
        }
        else {
            removeESPData();
            qDebug() << "ESP Entry FAILED\n";
            UdpSocket->writeDatagram(MSG_ANY_ERROR_VERIFICATION_TO_DATA, MSG_ANY_ERROR_VERIFICATION_TO_SIZE, sender_ip, sender_port);
        }
    }
    else if (target == CLIENT) {
        QString hash = generateUserHash(sender_ip, sender_port);

        if (datagram.data() == clients_data[hash].verification_code) {
            clients_data[hash].status = Connected;
            qDebug() << "Client Entry VERIFIED\n";
            UdpSocket->writeDatagram(MSG_ANY_CONNECTED_TO_DATA, MSG_ANY_CONNECTED_TO_SIZE,sender_ip, sender_port);
        }
        else {
            removeClientData(hash);
            qDebug() << "Client Entry FAILED\n";
            UdpSocket->writeDatagram(MSG_ANY_ERROR_VERIFICATION_TO_DATA, MSG_ANY_ERROR_VERIFICATION_TO_SIZE,sender_ip, sender_port);
        }

    }
    else {
        qDebug() << "Troubles in the end of verification!";
        qDebug() << "";
        UdpSocket->writeDatagram("Can't connect non-esp/client object", 35, sender_ip, sender_port);
    }
}


void UdpResender::readPendingDatagrams() {

    
    if (UdpSocket->hasPendingDatagrams()) {

        QHostAddress sender_ip;
        quint16 sender_port;

        datagram.resize(UdpSocket->pendingDatagramSize());
        UdpSocket->readDatagram(datagram.data(), datagram.size(), &sender_ip, &sender_port);

        UserType type = getUserType(sender_ip, sender_port);
        QString hash = generateUserHash(sender_ip, sender_port);

        if (datagram.data() == MSG_ANY_DISCONNECT_FROM_DATA) {
            disconnectUser(sender_ip, sender_port, type);
        }

        else if ((datagram.data() == MSG_ESP_ENTRY_FROM_DATA  && esp_data.status < Connected) || 
                (datagram.data() == MSG_CLIENT_ENTRY_DATA && clients_data[hash].status != Connected)) {
                
                clients_data.remove(hash);
                startVerificationProcess(sender_ip, sender_port);
            }

        else if (type == ESP) {
            bool have_clients = isAnyClientsConnected();
            
            if (datagram.data() == MSG_ESP_STATUS_REQUEST_FROM_DATA) {
                if (timer_req_ans->isActive())
                    timer_req_ans->stop();
                logEspMessage("Received ON_LINE!\n");
                qDebug() << "Received ON_LINE!\n";
            }
            
            else if (esp_data.status == Started)
                endVerificationProcess(sender_ip, sender_port, ESP);
            
            else if  (esp_data.status == Connected && !have_clients) {
                logEspMessage(QString("From" + esp_data.ip.toString() + "::" + QString::number(esp_data.port) + "\n\t" + datagram.data()));
                qDebug() << "@ Received msg from ESP: " << datagram.data();
                qDebug() << "ESP IPv6: " << esp_data.ip.toString();
                qDebug() << "ESP IPv4: " << esp_data.ip.toString().mid(7);
                qDebug() << "ESP PORT: " << esp_data.port;
                qDebug() << "But no CLIENT connected!";
                qDebug() << "";
            }
            
            else if (have_clients && esp_data.status == Connected) {
                logEspMessage(QString("From" + esp_data.ip.toString() + "::" + QString::number(esp_data.port) + "\n\t" + datagram.data()));
                qDebug() << "@ Received msg from ESP: " << datagram.data();
                qDebug() << "ESP IPv6: " << esp_data.ip.toString();
                qDebug() << "ESP IPv4: " << esp_data.ip.toString().mid(7);
                qDebug() << "ESP PORT: " << esp_data.port;
                qDebug() << "@ ReSended to the CLIENTS";
                qDebug() << "Number of clients: " << clients_data.size();
                qDebug() << "";
                customMulticast(datagram);
            }
        }

        else if (type == CLIENT) {
            if (clients_data[hash].status == Started)
                endVerificationProcess(sender_ip, sender_port, CLIENT);

            else if (clients_data[hash].status == Connected && datagram.data() == MSG_CLIENT_PING_FROM_DATA) {
                UdpSocket->writeDatagram(MSG_CLIENT_PING_TO_DATA, MSG_CLIENT_PING_TO_SIZE, sender_ip, sender_port);
            }

            else if  (clients_data[hash].status == Connected && esp_data.status < Connected) {
                qDebug() << "@ Received Msg from CLIENT: " << datagram.data();
                qDebug() << "CLIENT IPv6: " << sender_ip.toString();
                qDebug() << "CLIENT IPv4: " << sender_ip.toString().mid(7);
                qDebug() << "CLIENT PORT: " << sender_port;
                qDebug() << "But no ESP connected!";
                qDebug() << "";
                UdpSocket->writeDatagram(MSG_CLIENT_ERROR_NO_ESP_DATA, MSG_CLIENT_ERROR_NO_ESP_SIZE, sender_ip, sender_port);
            }
            
            else if (clients_data[hash].status == Connected && esp_data.status == Connected) {
                qDebug() << "@ Received msg from CLIENT: " << datagram.data();
                qDebug() << "CLIENT IPv6: " << sender_ip.toString();
                qDebug() << "CLIENT IPv4: " << sender_ip.toString().mid(7);
                qDebug() << "CLIENT PORT: " << sender_port;
                qDebug() << "@ ReSended to the ESP";
                qDebug() << "ESP IPv6: " << esp_data.ip.toString();
                qDebug() << "ESP IPv4: " << esp_data.ip.toString().mid(7);
                qDebug() << "ESP PORT: " << esp_data.port;
                qDebug() << "";
                UdpSocket->writeDatagram(datagram.data(), datagram.size(), esp_data.ip, esp_data.port);
            }
        }  

        else {

            qDebug() << "Sender IPv6: " << sender_ip.toString();
            qDebug() << "Sender IPv4: " << sender_ip.toString().mid(7);
            qDebug() << "Sender PORT: " << sender_port;
            qDebug() << "Size: " << datagram.size();
            qDebug() << "Data: " << datagram.data();
            qDebug() << "";

            // UdpSocket->writeDatagram(datagram.data(), datagram.size(), sender_ip, sender_port);
        }
    }
}
 
