# portfolio_cpp

`portfolio_cpp` ist ein C++17-Proof-of-Concept fuer einen Telemetrie- und Sensor-Service mit HTTP-API auf Basis von Crow. Laut [`handoff.md`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/handoff.md) ist das Projekt als taktischer Edge-Node-Mockup fuer ein spaeteres Linux-/ARM-Deployment gedacht. Der aktuelle Implementierungsstand bildet jedoch vor allem einen Windows-nativen Systemmonitor ab, der lokale Systemdaten per REST bereitstellt.

## Ziel laut Handoff

Das Handoff beschreibt ein Portfolio-Projekt fuer die Ruestungs-/Aerospace-Domaene:

- hardwarenahe Sensordatenerfassung auf einem Edge-Knoten
- Bereitstellung der Daten ueber eine REST-API
- spaeteres Dashboard fuer C4I-/Monitoring-Szenarien
- Zielplattform langfristig: Linux auf ARM, z. B. Raspberry Pi

Geplante Architektur:

1. Hardware-Layer: Sensoren und Systemdaten lesen, bei Bedarf mit Mocking/Fallback
2. Network-Layer: JSON-API mit Crow
3. Presentation-Layer: Frontend fuer Visualisierung und Polling

## Aktueller Ist-Zustand

Der aktuelle Code in [`main.cpp`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/main.cpp) implementiert einen Crow-Webservice mit Hintergrund-Threads fuer die Erfassung von Windows-Systemmetriken.

Erfasste Daten:

- CPU-Auslastung ueber `GetSystemTimes`
- Anzahl logischer CPU-Kerne
- CPU-Basisfrequenz aus der Windows-Registry
- RAM-Auslastung und Commit-Werte ueber `GlobalMemoryStatusEx`
- Festplattennutzung des Systemlaufwerks `C:`
- Uptime ueber `GetTickCount64`
- Temperaturzonen ueber WMI (`MSAcpi_ThermalZoneTemperature`)

Die Sensordaten werden in einem globalen Cache gehalten und ueber Crow-Handler als JSON ausgeliefert.

## API-Endpunkte

Die Anwendung startet standardmaessig auf Port `18080`. Der Port kann ueber die Umgebungsvariable `PORT` gesetzt werden.

- `GET /`
  Liefert Basisinformationen zum Service.
- `GET /api/sensors`
  Liefert einen Snapshot der aktuell gecachten Sensordaten.
- `GET /api/hello/<name>`
  Einfacher Test-Endpunkt fuer eine Begruessung.
- `GET /openapi.json`
  Liefert ein eingebettetes OpenAPI-3.0-Dokument.

Beispiel fuer `GET /api/sensors`:

```json
{
  "cpu": {
    "usage_percent": 12.3,
    "logical_cores": 8,
    "base_freq_mhz": 2400
  },
  "memory": {
    "total_mb": 16384.0,
    "available_mb": 9123.4,
    "used_percent": 44.0,
    "commit_total_mb": 32768.0,
    "commit_used_mb": 11800.0
  },
  "disk": {
    "drive": "C:",
    "total_gb": 476.0,
    "free_gb": 210.0,
    "used_percent": 55.9
  },
  "temperatures": [
    {
      "zone": "zone_0",
      "celsius": 48.2
    }
  ],
  "system": {
    "uptime_seconds": 12345.0
  }
}
```

## Build und Start

### Lokaler Build mit CMake

Die Build-Konfiguration in [`CMakeLists.txt`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/CMakeLists.txt) ist derzeit auf Windows und `vcpkg` zugeschnitten. Sie erwartet eine installierte Crow-Konfiguration ueber:

`C:/Users/Kirsc/vcpkg/scripts/buildsystems/vcpkg.cmake`

Beispiel:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\portfolio_cpp.exe
```

Je nach Generator kann das Binary auch direkt unter `build\portfolio_cpp.exe` liegen.

### Laufzeit

```powershell
$env:PORT="18080"
.\build\Release\portfolio_cpp.exe
```

Danach ist die API unter `http://localhost:18080` erreichbar.

## Docker-Stand

Ein [`Dockerfile`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/Dockerfile) und eine [`docker-compose.yml`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/docker-compose.yml) sind vorhanden. Inhaltlich zielen diese Dateien auf ein Linux-Container-Setup ab.

Wichtiger Hinweis:

- Der aktuelle Anwendungscode ist Windows-spezifisch und verwendet `windows.h`, WMI und WinAPI-Aufrufe.
- Das Dockerfile basiert auf Ubuntu.
- In der aktuellen Form passen Codebasis und Containerziel daher noch nicht zusammen.

Das bedeutet: Das Projekt hat bereits einen API-Prototypen, aber die Linux-/ARM-Ausrichtung aus dem Handoff ist noch nicht erreicht.

## Technische Einordnung

Aktuell ist das Projekt am treffendsten als Windows-basierter Telemetrie-API-Prototyp zu beschreiben:

- Crow ist integriert und die REST-API laeuft im Grundsatz
- Sensorwerte werden asynchron im Hintergrund gesammelt
- OpenAPI wird direkt vom Service ausgeliefert
- Die geplante Linux-/ARM-Sensorik aus dem Handoff ist noch nicht umgesetzt
- Ein Frontend ist im aktuellen Repository noch nicht enthalten

## Frontend-Entwicklung mit React

Es gibt jetzt einen separaten React-/Vite-Teil unter [`frontend/`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/frontend), der bewusst fuer die direkte UI-Entwicklung vom C++-Backend getrennt ist.

### Dev-Modus

1. Crow-Backend starten:

```powershell
.\build\Release\portfolio_cpp.exe
```

2. Frontend-Abhaengigkeiten mit `yarn` installieren:

```powershell
cd frontend
yarn install
```

3. Vite-Dev-Server starten:

```powershell
yarn dev
```

Danach laeuft das Frontend standardmaessig unter `http://localhost:5173`.

Wichtig:

- Requests auf `/api/*` und `/openapi.json` werden im Dev-Modus automatisch an `http://localhost:18080` weitergeleitet.
- UI-Aenderungen brauchen keinen C++-Rebuild.

### Produktions-/Demo-Build

Das Frontend kann fuer einen gemeinsamen Deploy mit Crow gebaut werden:

```powershell
cd frontend
yarn build
```

Dadurch entsteht `frontend/dist`. Wenn diese Build-Artefakte vorhanden sind, liefert Crow beim Aufruf von `/` die React-App aus. API-Endpunkte wie `/api/sensors` und `/openapi.json` bleiben dabei erhalten.

## Naechste sinnvolle Schritte

- Linux-kompatible Sensorabfragen fuer CPU, RAM, Temperatur und Dateisystem ergaenzen
- Windows-spezifische und Linux-spezifische Sensorquellen sauber kapseln
- Crow-Build vereinfachen, damit kein lokaler, hart codierter `vcpkg`-Pfad noetig ist
- Docker-Image auf den tatsaechlichen Zielcode abstimmen
- React-/Vite-Dashboard aus dem Handoff als dritte Schicht ergaenzen

## Projektdateien

- [`main.cpp`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/main.cpp): Crow-Service, Sensor-Caching, JSON-Ausgabe, OpenAPI
- [`CMakeLists.txt`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/CMakeLists.txt): CMake-Projektdefinition und Crow-Linking
- [`handoff.md`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/handoff.md): Zielbild, Kontext und Architekturrahmen
- [`Dockerfile`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/Dockerfile): geplanter Container-Build
- [`docker-compose.yml`](/C:/Users/Kirsc/CLionProjects/portfolio_cpp/docker-compose.yml): lokaler Container-Start
