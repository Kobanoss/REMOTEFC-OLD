#ifndef RESENDER_H 
#define RESENDER_H

#include <QObject>

#include <QString>
#include <QByteArray>
#include <QMap>
#include <QCryptographicHash>

#include <QUdpSocket>

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <QTimer>

#include <QFile>


enum UserType {ESP, CLIENT, OTHER_TARGET};
enum VerificationStatus {Disconnected, Started, Connected};
enum MessageType {SYSTEM, MSG, OTHER_MESSAGE};
enum SystemMessageType {STATE_CHANGER, WEATHER, OTHER_SYSTEM_MESSAGE};

typedef struct UserData {
    QHostAddress ip;
    quint16 port;
    UserType type;
    VerificationStatus status;
    QString verification_code;
} UserData;



class UdpResender : public QObject {
    Q_OBJECT
public:
    explicit UdpResender(QObject *parent = 0);
    ~UdpResender();
 
public slots:
    void readPendingDatagrams();
 
private:
    QByteArray datagram;
    
    UserData esp_data = {QHostAddress("0.0.0.0"), 0, ESP, Disconnected, ""};
    QMap<QString, UserData> clients_data = {}; 

    QTimer* timer_req_send;
    QTimer* timer_req_ans;
    QTimer* timer_req_metrics;
  
    void onStartup();
    void onTermination();

    void removeESPData();
    void removeClientData(QString hash_code);
    
    void sendStatusRequest();
    void sendMetricsRequest();
    void logEspMessage(QString data);

    UserType getUserType(QHostAddress sender_ip, quint16 sender_port);
    QString generateUserHash(QHostAddress sender_ip, quint16 sender_port);

    QString generateVerificationCode(QHostAddress sender_ip);
    void startVerificationProcess(QHostAddress sender_ip, quint16 sender_port);
    void endVerificationProcess(QHostAddress sender_ip, quint16 sender_port, UserType target);
    
    bool isAnyClientsConnected();
    void disconnectUser(QHostAddress sender_ip, quint16 sender_port, UserType target);

    void customMulticast(QByteArray msg);


    QUdpSocket* UdpSocket;
};

#endif // RESENDER_H