# INS4Droid

**INS4Droid** is a **lightweight, high-performance inertial navigation system (INS)** for Android that uses **built-in device sensors** to track position **without GNSS, GSM, Wi-Fi, or network connectivity**.

The system relies on an **initial frame of reference** and performs real-time inertial dead-reckoning using accelerometer and gyroscope data.  
It is implemented in **C with SDL2**, prioritizing speed, efficiency, and minimal overhead.

---

## Key Features

- **No external signals required**
  - Works without GPS, GSM, Wi-Fi, or internet
- **Pure inertial navigation**
  - Uses Android’s built-in IMU sensors
- **Super fast & lightweight**
  - Native C implementation with SDL2
- **High accuracy**
  - Average deviation **< 1 m per 2 km** (real-world testing)
- **Low overhead**
  - Suitable for long-running offline operation

---

## How It Works

1. An **initial frame of reference** is established at startup
2. Sensor data (accelerometer + gyroscope) is continuously integrated
3. Position is updated using inertial dead-reckoning
4. No external corrections or network dependencies are used

> Accuracy depends on sensor quality and initial calibration.

---

## Building

### Option 1: Android Studio (Recommended)

1. Clone the repository
2. Open **`android-project/`** in Android Studio
3. Let Gradle sync complete
4. Build and run on a physical Android device

---

### Option 2: Android Studio CLI Tools

Ensure the Android SDK, NDK, and CMake are installed.

```bash
cd android-project
./gradlew assembleDebug
