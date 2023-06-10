#include <Arduino.h>
#include <FlexiTimer2.h>
#include <digitalWriteFast.h>
#include <G2MotorDriver.h>

#define COMMAND_PREFIX "PR+"

// Codeur incrémental
#define NB_IMPULSIONS_PAR_TOUR 48
#define RAPPORT_DE_REDUCTION 20.4
#define DISTANCE_PAR_TOUR 0.8
#define codeurPinA 2
#define codeurPinB 3
volatile long ticksCodeur = 0;

// Moteur CC
#define DIRPin 5
#define PWMPin 9
#define SLPPin 4
#define FLTPin 6
#define CSPin A0
G2MotorDriver24v13 md(DIRPin, PWMPin, SLPPin, FLTPin, CSPin);

// Cadence d'échantillonnage en ms
#define CADENCE_MS 10
volatile double dt = CADENCE_MS / 1000.;

volatile double vitesse_reelle = 0.; // Vitesse de rotation réelle en rad/s
volatile double consigne_moteur = 0.; // Consigne envoyée au moteur
volatile double consigne_vitesse = 0.; // Consigne de vitesse en rad/s

// PID
volatile double Kp = 0.;
volatile double Ki = 0.;
volatile double Kd = 0.;
volatile double P_x = 0.;
volatile double I_x = 0.;
volatile double D_x = 0.;
volatile double erreur = 0.;
volatile double erreur_precedente = 0.;

volatile long totalTicks = 0;

bool log_speed = false;
bool asservissement_position = false;

void stopIfFault() {
    if (md.getFault()) {
        md.Sleep(); // put the driver to sleep on fault
        delay(1);
        Serial.println("Motor fault");
        while (true);
    }
}

// Routine de service d'interruption attachée à la voie A du codeur incrémental
void GestionInterruptionCodeurPinA() {
    if (digitalReadFast(codeurPinA) == digitalReadFast(codeurPinB)) {
        ticksCodeur--;

    } else {
        ticksCodeur++;

    }
}

// Routine de service d'interruption attachée à la voie B du codeur incrémental
void GestionInterruptionCodeurPinB() {
    if (digitalReadFast(codeurPinA) == digitalReadFast(codeurPinB)) {
        ticksCodeur++;

    } else {
        ticksCodeur--;

    }
}

void isrt() {
    long codeurDeltaPos;

    // Nombre de ticks codeur depuis la dernière fois
    codeurDeltaPos = ticksCodeur;
    ticksCodeur = 0;

    // Calcul de la vitesse de rotation
    vitesse_reelle = ((2. * 3.141592 * ((double) codeurDeltaPos)) / (NB_IMPULSIONS_PAR_TOUR * RAPPORT_DE_REDUCTION)) / dt; // en rad/s
    totalTicks += codeurDeltaPos;

    /******* Régulation PID ********/
    // Ecart entre la consigne et la mesure
    if (asservissement_position) {
        erreur = consigne_vitesse - totalTicks;
    } else {
        erreur = consigne_vitesse - vitesse_reelle;
    }

    // Terme proportionnel
    P_x = Kp * erreur;

    // Terme dérivé
    D_x = Kd * (erreur - erreur_precedente) / dt;

    // Calcul de la consigne_moteur
    consigne_moteur = P_x + I_x + D_x;

    // Terme intégral (sera utilisé lors du pas d'échantillonnage suivant)
    I_x = I_x + Ki * dt * erreur;

    // Mise à jour de l'erreur précédente
    erreur_precedente = erreur;

    /******* Fin régulation PID ********/

    // Envoi de la consigne_moteur au moteur
    if (consigne_vitesse == 0) {
        consigne_moteur = 0;
    }

    if (consigne_moteur > 400) {
        consigne_moteur = 400;
    } else if (consigne_moteur < -400) {
        consigne_moteur = -400;
    }

    md.setSpeed((int) consigne_moteur);
//    stopIfFault();

    if (log_speed) {
        Serial.print(COMMAND_PREFIX);
        Serial.print("speed=");
        Serial.println(vitesse_reelle);
    }
}

// Initialisations
void setup(void) {
    // Codeur incrémental
    pinMode(codeurPinA, INPUT); // entrée digitale pin A codeur
    pinMode(codeurPinB, INPUT); // entrée digitale pin B codeur
    digitalWrite(codeurPinA, HIGH); // activation de la résistance de pullup
    digitalWrite(codeurPinB, HIGH); // activation de la résistance de pullup
    attachInterrupt(digitalPinToInterrupt(codeurPinA), GestionInterruptionCodeurPinA, CHANGE);
    attachInterrupt(digitalPinToInterrupt(codeurPinB), GestionInterruptionCodeurPinB, CHANGE);

    // Moteur CC
    md.init();
    md.Wake(); // Wake the driver for current readings
    md.calibrateCurrentOffset();
    delay(10);

    // Liaison série
    Serial.begin(115200);

    // Compteur d'impulsions de l'encodeur
    ticksCodeur = 0;

    // La routine isrt est exécutée à cadence fixe
    FlexiTimer2::set(CADENCE_MS, 1 / 1000., isrt); // résolution timer = 1 ms
    FlexiTimer2::start();
}


void process_commands() {
    if (Serial.available() <= 0) return; // No data available

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith(COMMAND_PREFIX)) {
        command = command.substring(strlen(COMMAND_PREFIX));

        String command_name;
        String command_args;

        if (command.indexOf('=') == -1) {
            command_name = command;
            command_args = "";
        } else {
            command_name = command.substring(0, command.indexOf('='));
            command_args = command.substring(command.indexOf('=') + 1);
        }

        if (command_name == "ping") {
            Serial.print(COMMAND_PREFIX);
            Serial.println("pong");
        } else if (command_name == "kp") {
            Kp = command_args.toFloat();
            Serial.print(COMMAND_PREFIX);
            Serial.print("Kp=");
            Serial.println(Kp);
        } else if (command_name == "ki") {
            Ki = command_args.toFloat();
            Serial.print(COMMAND_PREFIX);
            Serial.print("Ki=");
            Serial.println(Ki);
        } else if (command_name == "kd") {
            Kd = command_args.toFloat();
            Serial.print(COMMAND_PREFIX);
            Serial.print("Kd=");
            Serial.println(Kd);
        } else if (command_name == "consigne") {
            if (asservissement_position) {
                consigne_vitesse = command_args.toFloat() * NB_IMPULSIONS_PAR_TOUR * RAPPORT_DE_REDUCTION / DISTANCE_PAR_TOUR;
            } else {
                consigne_vitesse = command_args.toFloat();
            }
            Serial.print(COMMAND_PREFIX);
            Serial.print("consigne=");
            Serial.println(consigne_vitesse);
        } else if (command_name == "log") {
            log_speed = command_args.toInt() != 0;
            Serial.print(COMMAND_PREFIX);
            Serial.print("log=");
            Serial.println(log_speed);
        } else if (command_name == "reset") {
            erreur = 0.;
            erreur_precedente = 0.;
            P_x = 0.;
            I_x = 0.;
            D_x = 0.;
            consigne_moteur = 0.;
            consigne_vitesse = 0.;
            vitesse_reelle = 0.;
            ticksCodeur = 0;
            totalTicks = 0;
            md.setSpeed(0);
            Serial.print(COMMAND_PREFIX);
            Serial.println("reset");
        } else if (command_name == "asservissement_position") {
            asservissement_position = command_args.toInt() != 0;
            Serial.print(COMMAND_PREFIX);
            Serial.print("asservissement_position=");
            Serial.println(asservissement_position);
        }
    }
}

void loop() {
    process_commands();
}