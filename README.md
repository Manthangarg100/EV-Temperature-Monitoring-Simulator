# VoltGuard – EV Battery Temperature Monitoring Simulator

VoltGuard is a web-based **EV Battery Temperature Monitoring Simulator** that demonstrates how battery temperature can change under different operating and cooling conditions. Users can adjust simulation parameters, run a thermal simulation, view the temperature response, and inspect saved simulation readings.

> **Project type:** Academic / educational simulator  
> **Domain:** Electric Vehicles, Battery Thermal Management, Web Application  
> **Backend language:** C  
> **Frontend:** HTML, CSS, JavaScript  
> **Database:** SQLite  
> **Deployment:** Render

---

## Live Demo

- **Live website (share this link):** https://ev-battery-monitor-ujgw.onrender.com
- **Backend API:** https://ev-battery-backend-rsy2.onrender.com
- **API health check:** https://ev-battery-backend-rsy2.onrender.com/api/health

The website link is the main link to share with your professor or evaluators. The backend links are provided for technical/API verification.

> **Hosting note:** The backend is hosted on Render's free service. It may take a short time to respond after being idle because the service can sleep. Data stored in the service's local SQLite file may not survive service restarts or redeployments.

---

## Features

- Interactive battery-temperature simulation.
- User-adjustable simulation inputs, including:
  - Battery current
  - Ambient temperature
  - Initial battery temperature
  - Cooling coefficient
  - Simulation duration
- Thermal model that estimates battery temperature over time.
- Temperature safety-status classification:
  - **LOW:** below 5 °C
  - **NORMAL:** 5 °C to below 45 °C
  - **WARNING:** 45 °C to below 60 °C
  - **CRITICAL:** 60 °C and above
- Temperature chart for viewing the simulation response.
- Simulation history stored using SQLite.
- History viewing and reset/clear-history functionality.
- C-based HTTP API connected to a browser-based dashboard.

**Important:** VoltGuard is an educational simulator. It does not connect to a real EV battery, physical temperature sensor, Battery Management System (BMS), or vehicle CAN bus. Its output is based on the implemented mathematical model and user-provided inputs.

---

## Technology Stack

| Layer | Technology |
|---|---|
| Backend | C (C11) |
| HTTP server | C with socket-based networking |
| Compiler | GCC |
| Database | SQLite |
| Frontend | HTML, CSS, JavaScript |
| Charts | Browser-side chart visualization |
| Hosting | Render |
| API communication | HTTP / JSON |

---

## How It Works

1. The user opens the VoltGuard dashboard.
2. The user sets the simulation parameters using the available controls.
3. The frontend sends the selected values to the C backend through the `/api/simulate` endpoint.
4. The backend applies the thermal calculation and generates simulation results.
5. The frontend displays the resulting temperature information and chart.
6. The backend stores available readings in SQLite so that the history endpoint can return them.
7. The user can view the saved history or clear it using the dashboard controls.

### Thermal Model

The simulator uses a simplified heat-generation and cooling model.

Electrical heat generation:

\[
P_{\text{heat}} = I^2R
\]

where:

- \(I\) is the battery current in amperes.
- \(R\) is the assumed internal resistance in ohms.

Cooling power:

\[
P_{\text{cooling}} = h(T - T_{\text{ambient}})
\]

where:

- \(h\) is the cooling coefficient.
- \(T\) is the estimated battery temperature.
- \(T_{\text{ambient}}\) is the ambient temperature.

The implementation uses these model constants:

- Assumed internal resistance: **0.045 Ω**
- Thermal mass: **280 kg**
- Specific heat capacity: **900 J/(kg·K)**

These are simulator assumptions, not measurements from a physical battery pack. The model is intended for demonstration and learning, not for engineering certification or real-world battery safety decisions.

---

## API Endpoints

Base URL:

```text
https://ev-battery-backend-rsy2.onrender.com
```

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/health` | Checks whether the backend API is responding. |
| POST | `/api/simulate` | Runs a battery-temperature simulation using submitted parameters. |
| GET | `/api/readings` | Retrieves saved simulation readings. |
| POST | `/api/reset` | Resets/clears the stored simulation history. |

### Health Check

Open this URL in a browser:

```text
https://ev-battery-backend-rsy2.onrender.com/api/health
```

A successful response should look similar to:

```json
{
  "ok": true,
  "service": "EV Battery C API"
}
```

---

## Project Structure

The repository is organized into separate frontend and backend folders.

```text
EV-Battery-Temperature-Monitoring-Simulator/
├── backend/
│   ├── main.c
│   └── ...                 # Backend build/deployment files, if included
│
├── frontend/
│   ├── index.html          # Main dashboard page
│   ├── style.css           # Dashboard styling
│   ├── app.js              # Frontend interactions and API calls
│   ├── config.js           # Backend API base URL
│   └── vercel.json         # Frontend hosting configuration, if retained
│
└── README.md
```

The exact additional files may vary depending on the version committed to the repository.

---

## Run Locally

### Requirements

- Windows, Linux, or macOS
- GCC C compiler
- SQLite development library and headers
- A modern web browser
- Python 3 (optional, for serving the frontend locally)

### 1. Compile the C backend

Open a terminal in the `backend` directory.

On a system where GCC and SQLite development files are installed, compile with:

```bash
gcc -std=c11 -O2 -Wall -Wextra main.c -lsqlite3 -lm -o ev-battery-api
```

### 2. Start the backend

On Linux/macOS:

```bash
./ev-battery-api
```

On Windows, run the generated executable from the terminal:

```powershell
.\ev-battery-api.exe
```

The server uses the `PORT` environment variable when provided and otherwise defaults to port `8080`.

### 3. Configure the frontend

Open `frontend/config.js` and set the API base URL to the local backend address:

```javascript
window.APP_CONFIG = {
  API_BASE: "http://localhost:8080"
};
```

For the deployed website, the API base URL should point to the deployed backend:

```javascript
window.APP_CONFIG = {
  API_BASE: "https://ev-battery-backend-rsy2.onrender.com"
};
```

Use the appropriate value for the environment you are running.

### 4. Open the frontend

You can open `frontend/index.html` in a browser. If browser restrictions affect API requests, serve the frontend through a local development server instead. For example, from the `frontend` directory:

```bash
python -m http.server 5500
```

Then open:

```text
http://localhost:5500
```

---

## Deployment Overview

The project is deployed on Render as two services:

1. **Backend Web Service:** runs the compiled C API.
2. **Frontend Static Site:** serves the HTML, CSS, and JavaScript dashboard.

The frontend's `config.js` must contain the public backend URL so the dashboard can send API requests to the C service.

No Docker container or WebAssembly backend is required for this deployment.

---

## Testing Checklist

Use this checklist when demonstrating the project:

- [ ] Open the live website.
- [ ] Confirm that the dashboard loads.
- [ ] Change one or more simulation inputs.
- [ ] Run a simulation and confirm that result values are displayed.
- [ ] Confirm that the temperature chart updates.
- [ ] Open the history section and check saved readings.
- [ ] Test the clear/reset history control.
- [ ] Open the backend health-check URL and confirm that the API responds.

Only mark a test as complete after personally verifying it in the deployed application.

---

## Limitations and Future Enhancements

### Current limitations

- The application is a mathematical simulator, not a physical battery-monitoring device.
- The thermal model uses simplified assumptions and fixed model constants.
- It does not acquire readings from real sensors or an EV battery pack.
- Render free hosting may sleep when idle.
- Local SQLite data on a free hosted instance may be lost after a restart or redeployment.

### Possible future enhancements

- Calibrate the thermal model using documented battery test data.
- Add downloadable simulation reports.
- Add user accounts and individual simulation histories.
- Use persistent managed database storage for production deployment.
- Add validated battery-pack parameters for different battery chemistries and configurations.
- Integrate physical sensors and a BMS only as a separately engineered and safety-validated extension.

---

## Academic Project Information

- **Project:** EV Battery Temperature Monitoring Simulator
- **Project name/brand:** VoltGuard
- **Student:** Manthan Garg
- **Roll number:** 24BCS10883
- **Programme:** B.Tech
- **Branch:** Computer Science and Engineering
- **University:** Chandigarh University
- **Academic session:** 2026–2027
- **Project guide:** Rahul Duragapal, Assistant Professor


---

## Author

**Manthan Garg**  
B.Tech – Computer Science and Engineering  
Chandigarh University

---

## Disclaimer

VoltGuard is developed for academic demonstration and educational purposes. It is not a certified battery-management, diagnostic, or safety system. Do not use its simulated results to operate, charge, diagnose, or make safety decisions about a real electric-vehicle battery.
