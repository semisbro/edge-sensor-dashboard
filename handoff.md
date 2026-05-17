# AI Handoff & Project Context: Tactical Edge Node Mockup

## 🎯 Projektziel
Dieses Projekt ist ein Proof of Concept (PoC) für eine Bewerbung in der Rüstungs-/Aerospace-Industrie (Zielunternehmen u.a. Elbit Systems, Thales).
Es simuliert einen hardwarenahen Edge-Sensor-Knoten (Ziel-Hardware: ARM-basierter Raspberry Pi unter Linux), der Telemetriedaten erfasst und via REST-API an ein Command-Dashboard (C4I) streamt.

## 🛠️ Tech Stack & Constraints
- **Sprache:** Modern C++ (Strikt C++17 Standard!)
- **Target OS:** Linux (Fokus auf ARM-Architektur / Raspberry Pi)
- **API Framework:** Crow (C++ Microframework)
- **Frontend:** React + Vite.js (Dark Mode, taktisches UI)
- **Deployment:** Docker (Multi-Stage Build für schlanke Container)
- **Build System:** CMake

## 🏗️ Architektur (Die 3 Schichten)
1. **Hardware-Layer (C++):** Auslesen echter Systemdaten (z. B. `/sys/class/thermal/...`) mit Fallback auf simulierte/gemockte Werte (GPS-Koordinaten, Signalstärke), falls die Hardware-Sensoren lokal nicht verfügbar sind.
2. **Network-Layer (C++ / Crow):** Bereitstellung der Sensordaten über asynchrone REST-Endpoints (JSON-Format).
3. **Presentation-Layer (React):** Regelmäßiges Polling der Endpoints und Visualisierung der Daten in Echtzeit.

## 🚨 System-Prompt & Regeln für die KI-Assistenz
Wenn du Code für dieses Projekt generierst oder refaktorierst, halte dich an folgende Regeln:
1. **Anti-Junior Code:** Schreibe produktionsreifen, robusten Code. Nutze Smart Pointer statt roher Pointer. Vermeide Memory Leaks.
2. **Kein Overengineering:** Es ist ein PoC. Wir brauchen keine Datenbank und kein Kafka-Cluster. In-Memory-Mocking und einfache HTTP-Requests reichen völlig.
3. **Error Handling & Logging:** Systemaufrufe (wie das Lesen von Dateien) müssen fehlertolerant sein (`try-catch`, Überprüfung auf `is_open()`). Nutze professionelles Konsolen-Logging im Style von `[SYS-LOG] [INFO] CPU Temp read successful`.
4. **C++17 Konformität:** Nutze keine Features aus C++20 oder neuer. Der Code muss auf etablierten, sicherheitskritischen Toolchains (wie z. B. für VxWorks oder ältere GCC-Compiler) kompilierbar sein.
5. **Kompakte Antworten:** Erkläre mir nicht die Grundlagen von C++. Gib mir direkt den optimierten Code und erkläre nur Architektur-Entscheidungen oder spezifische Workarounds für Linux/ARM.

## 📍 Aktueller Status
- [x] Phase 1: Core C++ Mocking-Logik (Temperatur & GPS Generator) steht.
- [ ] Phase 2: Integration des Crow API-Frameworks und JSON-Serialisierung.
- [ ] Phase 3: Dockerfile (Multi-Stage) schreiben.
- [ ] Phase 4: React Dashboard bauen.