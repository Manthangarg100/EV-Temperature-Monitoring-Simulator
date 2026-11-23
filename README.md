# VoltGuard — EV Battery Temperature Monitoring Simulator

This version keeps the **simulation engine and REST API in C**. The web page is HTML/CSS/JavaScript only for presentation and HTTP requests.

## Deployment without Docker commands or a Dockerfile

- Frontend: Vercel
- C backend: Railway using Railpack build/start commands
- Database: SQLite (starter database is local to the backend service)

Railway's builder packages the service internally; **you do not install or run Docker Desktop, write a Dockerfile, or use Docker commands**.

## Project structure

```text
EV_Battery_Temperature_Monitoring_C_Vercel_Railway/
├── backend/
│   ├── main.c          # C HTTP server, thermal simulation, SQLite API
│   ├── Procfile        # Railway web process hint
│   └── CMakeLists.txt
└── frontend/
    ├── index.html      # Dashboard
    ├── style.css       # Responsive UI
    ├── app.js          # Calls the C API and renders charts/history
    ├── config.js       # Set the deployed C API URL here
    └── vercel.json
```

## 1. Deploy the C backend on Railway

1. Push the complete project to a GitHub repository.
2. Open https://railway.com and create a project from that GitHub repository.
3. Add/select the service for this repository.
4. In service Settings, set **Root Directory** to `/backend`.
5. Set the **Build Command** to:

```bash
gcc -std=c11 -O2 -Wall -Wextra main.c -lsqlite3 -lm -o ev-battery-api
```

6. Set the **Start Command** to:

```bash
./ev-battery-api
```

7. Add these service variables (space-separated package names):

```text
RAILPACK_BUILD_APT_PACKAGES=... gcc libsqlite3-dev
RAILPACK_DEPLOY_APT_PACKAGES=... libsqlite3-0
```

The first installs the C compiler and SQLite headers during build. The second supplies the SQLite shared library at runtime. Railway/Railpack documents custom build/start commands and additional Apt packages.

8. Deploy the service. In Settings → Networking, generate a public domain.
9. Test this URL in a browser:

```text
https://YOUR-RAILWAY-DOMAIN/api/health
```

Expected response:

```json
{"ok":true,"service":"EV Battery C API"}
```

Railway's free plan currently includes $1 of monthly usage and services sleep when idle; a new account also receives a 30-day trial with a one-time $5 credit. The backend may sleep when idle, so the first request after a quiet period can take longer. Check current usage in Railway before relying on it for a long-running public demo.

## 2. Connect the frontend to the C backend

Open `frontend/config.js` and replace the local URL with the public Railway URL:

```js
window.APP_CONFIG = {
  API_BASE: "https://YOUR-RAILWAY-DOMAIN"
};
```

Do not add a trailing slash. Commit and push this change to GitHub.

## 3. Deploy the frontend on Vercel

1. Open https://vercel.com/new and import the same GitHub repository.
2. Set **Root Directory** to `frontend`.
3. Framework preset: **Other**.
4. Build command: leave empty.
5. Output directory: `.` (or the project root as Vercel's static output directory).
6. Deploy.

Your public Vercel URL is the demo link to share. The frontend sends simulation requests to the separate C backend.

## 4. Local demo on Windows

The supplied C server uses POSIX sockets, so the simplest Windows setup is **WSL (Ubuntu)** or a Linux terminal with GCC and SQLite development headers. In WSL, from `backend`:

```bash
sudo apt update
sudo apt install gcc libsqlite3-dev
gcc -std=c11 -O2 -Wall -Wextra main.c -lsqlite3 -lm -o ev-battery-api
./ev-battery-api
```

It listens on port 8080 by default. Open the frontend with VS Code Live Server and keep `frontend/config.js` set to `http://localhost:8080`.

## 5. API endpoints

| Method | Endpoint | Description |
|---|---|---|
| GET | `/api/health` | C backend health check |
| POST | `/api/simulate` | Calculate and save one thermal simulation |
| GET | `/api/readings` | Read the latest 100 SQLite records |
| POST | `/api/reset` | Clear simulation history |

Example simulation request:

```json
{
  "current": 250,
  "ambient": 35,
  "initialTemp": 30,
  "cooling": 22,
  "duration": 120
}
```

## 6. How the C thermal model works

The backend uses a simplified lumped thermal model:

- Resistive heat: `P_heat = I² × R`
- Cooling: `P_cooling = h × (T_battery − T_ambient)`
- Temperature change: `ΔT = (P_heat − P_cooling) × Δt / (m × Cp)`

Illustrative constants: `R = 0.045 Ω`, `m = 280 kg`, `Cp = 900 J/(kg·K)`. Demo status thresholds: below 5°C LOW; 5–44°C NORMAL; 45–59°C WARNING; 60°C or higher CRITICAL.

## Important limitations

This is an educational simulator, not a physical battery monitoring device. It does not connect to EV sensors, a BMS or a CAN bus. The thermal model and thresholds are illustrative and are not certified for engineering or safety decisions.

SQLite data on an ephemeral service filesystem can be lost when the service is redeployed or restarted. For a short academic demonstration this may be acceptable; if durable history is required, attach a persistent volume and update the database path in the C program to the volume mount path.
