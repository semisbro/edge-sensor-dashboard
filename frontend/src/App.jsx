import { useState, useEffect } from "react";
import { useQuery } from "@tanstack/react-query";
import { sensorsQueryOptions } from "./api/sensors";
import { disksQueryOptions } from "./api/disks";
import { MOCK_SENSORS, MOCK_DISKS } from "./mockData";
import { pingQueryOptions } from "./api/ping";
import { CROW_SERVER_URL } from "./config";

// ─── Formatters ───────────────────────────────────────────────────────────────

function fmt(value, digits = 1) {
  return typeof value === "number" && !Number.isNaN(value)
    ? value.toFixed(digits)
    : "–";
}

function fmtCache(kb) {
  if (!kb) return "–";
  return kb >= 1024 ? `${(kb / 1024).toFixed(0)} MB` : `${kb} KB`;
}

function fmtUptime(seconds) {
  if (typeof seconds !== "number" || Number.isNaN(seconds)) return "–";
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m`;
}

function tempTone(celsius) {
  if (celsius >= 80) return "danger";
  if (celsius >= 60) return "warning";
  return "ok";
}

function tempLabel(celsius) {
  if (celsius >= 80) return "Critical";
  if (celsius >= 60) return "Warm";
  return "Normal";
}

function diskTone(pct) {
  if (pct >= 90) return "danger";
  if (pct >= 75) return "warning";
  return "ok";
}

function cpuTone(pct) {
  if (pct >= 90) return "danger";
  if (pct >= 70) return "warning";
  return "ok";
}

function memTone(pct) {
  if (pct >= 90) return "danger";
  if (pct >= 75) return "warning";
  return "ok";
}

function getStatus({ isLoading, isFetching, isError, hasData, error }) {
  if (isLoading && !hasData) return { label: "Connecting…", tone: "neutral" };
  if (isError && hasData)
    return { label: "Stale – refresh failed", tone: "warning", detail: error?.message };
  if (isError)
    return { label: "Backend offline", tone: "danger", detail: error?.message };
  if (isFetching) return { label: "Refreshing", tone: "active" };
  return { label: "Live", tone: "success" };
}

// ─── Progress Bar ─────────────────────────────────────────────────────────────

function ProgressBar({ value, tone = "ok", className = "" }) {
  const clamped = Math.min(100, Math.max(0, value || 0));
  return (
    <div className={`progress-track ${className}`}>
      <div
        className={`progress-fill progress-fill--${tone}`}
        style={{ width: `${clamped}%` }}
      />
    </div>
  );
}

// ─── Stat Card ────────────────────────────────────────────────────────────────

function StatCard({ label, value, suffix, accent = "blue", sub, progress, progressTone }) {
  return (
    <article className={`stat-card stat-card--${accent}`}>
      <span className="stat-label">{label}</span>
      <strong className="stat-value">
        {value}
        {suffix != null && <span className="stat-suffix">{suffix}</span>}
      </strong>
      {sub && <span className="stat-sub">{sub}</span>}
      {progress !== undefined && (
        <ProgressBar value={progress} tone={progressTone || "ok"} />
      )}
    </article>
  );
}

// ─── Disk Card ────────────────────────────────────────────────────────────────

function DiskCard({ disk }) {
  const tone = diskTone(disk.used_percent);
  const badgeTone = disk.ready ? "ok" : "danger";
  const typeLabel = disk.drive_type ?? "unknown";

  return (
    <article className="disk-card">
      <div className="disk-card__header">
        <span className="disk-card__mount">{disk.mount_point}</span>
        <span className={`badge badge--${badgeTone}`}>
          {disk.ready ? typeLabel : "not ready"}
        </span>
      </div>

      <span className="disk-card__meta">
        {[disk.volume_label, disk.filesystem].filter(Boolean).join(" · ") || "No label"}
      </span>

      <div className="disk-card__stats">
        <div className="disk-stat">
          <span className="disk-stat__label">Total</span>
          <span className="disk-stat__value">{fmt(disk.total_gb)} GB</span>
        </div>
        <div className="disk-stat">
          <span className="disk-stat__label">Used</span>
          <span className="disk-stat__value">{fmt(disk.used_gb)} GB</span>
        </div>
        <div className="disk-stat">
          <span className="disk-stat__label">Free</span>
          <span className="disk-stat__value">{fmt(disk.free_gb)} GB</span>
        </div>
      </div>

      <ProgressBar value={disk.used_percent} tone={tone} />
      <div className="disk-card__footer">
        <span className="disk-card__pct">{fmt(disk.used_percent)}% used</span>
        <span className={`badge badge--${tone === "ok" ? "neutral" : tone}`}>
          {disk.used_percent >= 90 ? "Critical" : disk.used_percent >= 75 ? "High" : "Healthy"}
        </span>
      </div>
    </article>
  );
}

// ─── Thermal Card ─────────────────────────────────────────────────────────────

function ThermalCard({ zone, celsius }) {
  const tone = tempTone(celsius);
  const progress = Math.min(100, (celsius / 100) * 100);
  return (
    <article className={`thermal-card thermal-card--${tone}`}>
      <span className="thermal-zone">{zone}</span>
      <strong className="thermal-temp">
        {fmt(celsius, 1)}
        <span className="thermal-unit">°C</span>
      </strong>
      <ProgressBar value={progress} tone={tone} />
      <span className="thermal-status">{tempLabel(celsius)}</span>
    </article>
  );
}

// ─── Info Item ────────────────────────────────────────────────────────────────

function InfoItem({ label, value, mono = false, tone }) {
  let cls = "info-item__value";
  if (mono)  cls += " info-item__value--mono";
  if (tone)  cls += ` info-item__value--${tone}`;
  return (
    <div className="info-item">
      <span className="info-item__label">{label}</span>
      <span className={cls}>{value ?? "–"}</span>
    </div>
  );
}

// ─── Server Status ────────────────────────────────────────────────────────────

function ServerStatus({ ping }) {
  const online    = ping.data?.online === true;
  const latency   = ping.data?.latencyMs;
  const offline   = ping.isError;
  const checking  = ping.isLoading;

  const tone = offline ? "offline" : online ? "online" : "checking";

  return (
    <div className={`server-status server-status--${tone}`}>
      <span className={`server-dot server-dot--${tone}`} />
      <span className="server-label">Crow</span>
      <span className="server-divider" />
      <span className="server-url">{CROW_SERVER_URL}</span>
      {online   && latency != null && <span className="server-ping">{latency} ms</span>}
      {offline  && <span className="server-ping server-ping--err">offline</span>}
      {checking && <span className="server-ping server-ping--muted">…</span>}
    </div>
  );
}

// ─── Navbar ───────────────────────────────────────────────────────────────────

const TABS = [
  { id: "overview", label: "Overview", icon: "⬡" },
  { id: "disks",    label: "Disks",    icon: "◫" },
  { id: "thermal",  label: "Thermal",  icon: "◈" },
  { id: "system",   label: "System",   icon: "◉" },
];

function Navbar({ theme, onThemeToggle, activeTab, onTabChange, mockMode, onMockToggle, ping }) {
  return (
    <header className="navbar">
      <div className="navbar__brand">
        <div className="navbar__logo">EN</div>
        <span className="navbar__title">Edge Node</span>
        <span className="navbar__tag">Dashboard</span>
      </div>

      <nav className="navbar__tabs">
        {TABS.map((tab) => (
          <button
            key={tab.id}
            className={`nav-tab${activeTab === tab.id ? " nav-tab--active" : ""}`}
            onClick={() => onTabChange(tab.id)}
            type="button"
          >
            <span className="nav-tab__icon">{tab.icon}</span>
            {tab.label}
          </button>
        ))}
      </nav>

      <div className="navbar__right">
        <ServerStatus ping={ping} />
        <button
          className={`mock-toggle${mockMode ? " mock-toggle--active" : ""}`}
          onClick={onMockToggle}
          type="button"
          title="Toggle demo mode for presentations"
        >
          <span className="mock-toggle__dot" />
          Demo
        </button>
        <button
          className="theme-toggle"
          onClick={onThemeToggle}
          type="button"
          title={`Switch to ${theme === "dark" ? "light" : "dark"} mode`}
        >
          {theme === "dark" ? "☀" : "◑"}
        </button>
      </div>
    </header>
  );
}

// ─── Footer ───────────────────────────────────────────────────────────────────

function Footer({ lastUpdate, isFetching, mockMode }) {
  return (
    <footer className="footer">
      <div className="footer__left">
        <span className="footer__stack">Crow C++ · Asio · React 18 · Vite</span>
        <span className="footer__sep">·</span>
        <a className="footer__link" href="/api/meta" target="_blank" rel="noreferrer">
          /api/meta
        </a>
        <span className="footer__sep">·</span>
        <a className="footer__link" href="/openapi.json" target="_blank" rel="noreferrer">
          /openapi.json
        </a>
      </div>

      <div className="footer__right">
        {mockMode ? (
          <div className="live-pill live-pill--demo">
            <span className="live-dot" />
            <span className="live-text">Demo · Static</span>
          </div>
        ) : (
          <div className="live-pill">
            <span className={`live-dot${isFetching ? " live-dot--pulse" : ""}`} />
            <span className="live-text">Live · 2s</span>
          </div>
        )}
        <span className="footer__time">
          {mockMode
            ? "Simulated data"
            : lastUpdate
              ? `Updated ${new Date(lastUpdate).toLocaleTimeString()}`
              : "Waiting for data…"}
        </span>
      </div>
    </footer>
  );
}

// ─── Overview Tab ─────────────────────────────────────────────────────────────

function OverviewTab({ data }) {
  const cpu    = data?.cpu    ?? {};
  const memory = data?.memory ?? {};
  const disk   = data?.disk   ?? {};
  const system = data?.system ?? {};

  const memUsedMb    = (memory.total_mb    ?? 0) - (memory.available_mb ?? 0);
  const commitUsedPct =
    memory.commit_total_mb > 0
      ? (memory.commit_used_mb / memory.commit_total_mb) * 100
      : 0;

  return (
    <>
      <div className="section-head">
        <h2>System Metrics</h2>
        <span className="section-badge">/api/sensors</span>
      </div>

      <div className="grid grid--hero">
        <StatCard
          label="CPU Usage"
          value={fmt(cpu.usage_percent)}
          suffix="%"
          accent="blue"
          sub={`${cpu.logical_cores ?? "–"} cores · ${cpu.base_freq_mhz ?? "–"} MHz base`}
          progress={cpu.usage_percent}
          progressTone={cpuTone(cpu.usage_percent)}
        />
        <StatCard
          label="Memory Used"
          value={fmt(memory.used_percent)}
          suffix="%"
          accent="teal"
          sub={`${fmt(memUsedMb, 0)} / ${fmt(memory.total_mb, 0)} MB`}
          progress={memory.used_percent}
          progressTone={memTone(memory.used_percent)}
        />
        <StatCard
          label={`Disk (${disk.drive ?? "C:"})`}
          value={fmt(disk.used_percent)}
          suffix="%"
          accent="rose"
          sub={`${fmt(disk.free_gb)} GB free of ${fmt(disk.total_gb)} GB`}
          progress={disk.used_percent}
          progressTone={diskTone(disk.used_percent)}
        />
        <StatCard
          label="Uptime"
          value={fmtUptime(system.uptime_seconds)}
          accent="slate"
          sub="system uptime"
        />
      </div>

      <div className="section-spacer" />

      <div className="section-head">
        <h2>Detail Breakdown</h2>
      </div>

      <div className="grid grid--detail">
        <StatCard
          label="Base Clock"
          value={cpu.base_freq_mhz ?? "–"}
          suffix=" MHz"
          accent="amber"
        />
        <StatCard
          label="Logical Cores"
          value={cpu.logical_cores ?? "–"}
          accent="slate"
        />
        <StatCard
          label="RAM Available"
          value={fmt(memory.available_mb, 0)}
          suffix=" MB"
          accent="teal"
        />
        <StatCard
          label="Commit Used"
          value={fmt(memory.commit_used_mb, 0)}
          suffix=" MB"
          accent="indigo"
          sub={`of ${fmt(memory.commit_total_mb, 0)} MB limit`}
          progress={commitUsedPct}
          progressTone={memTone(commitUsedPct)}
        />
        <StatCard
          label="Disk Free"
          value={fmt(disk.free_gb)}
          suffix=" GB"
          accent="rose"
        />
      </div>
    </>
  );
}

// ─── Disks Tab ────────────────────────────────────────────────────────────────

function DisksTab({ data, isLoading, isError }) {
  if (isLoading) {
    return (
      <div className="empty-state">
        <div className="empty-state__icon">◫</div>
        <p className="empty-state__title">Loading disk inventory…</p>
      </div>
    );
  }

  if (isError) {
    return (
      <div className="empty-state">
        <div className="empty-state__icon">✕</div>
        <p className="empty-state__title">Failed to load disk inventory</p>
        <p className="empty-state__sub">Check that the Crow backend is running on port 18080.</p>
      </div>
    );
  }

  const disks = data?.disks ?? [];

  if (!disks.length) {
    return (
      <div className="empty-state">
        <div className="empty-state__icon">◫</div>
        <p className="empty-state__title">No disks reported</p>
        <p className="empty-state__sub">The backend returned an empty disk inventory.</p>
      </div>
    );
  }

  return (
    <>
      <div className="section-head">
        <h2>Disk Inventory</h2>
        <span className="section-badge">/api/disks</span>
        <span className="section-badge" style={{ background: "var(--surface-2)", color: "var(--muted)" }}>
          {data.count} {data.count === 1 ? "drive" : "drives"}
        </span>
      </div>
      <div className="disk-grid">
        {disks.map((disk) => (
          <DiskCard key={disk.mount_point} disk={disk} />
        ))}
      </div>
    </>
  );
}

// ─── Thermal Tab ──────────────────────────────────────────────────────────────

function ThermalTab({ temperatures }) {
  if (!temperatures?.length) {
    return (
      <div className="empty-state">
        <div className="empty-state__icon">◈</div>
        <p className="empty-state__title">No thermal zones reported</p>
        <p className="empty-state__sub">
          On Linux, thermal zones are read from /sys/class/thermal — none detected (VM or unsupported kernel).
          On macOS, SMC thermal access requires a third-party driver.
          On Windows, run the backend as Administrator to enable WMI thermal zones.
        </p>
      </div>
    );
  }

  const hottest = [...temperatures].sort((a, b) => b.celsius - a.celsius)[0];

  return (
    <>
      <div className="section-head">
        <h2>Thermal Zones</h2>
        <span className="section-badge">/api/sensors · temperatures</span>
        <span className="section-badge" style={{ background: "var(--surface-2)", color: "var(--muted)" }}>
          {temperatures.length} {temperatures.length === 1 ? "zone" : "zones"}
        </span>
      </div>

      {hottest && hottest.celsius >= 60 && (
        <div className={`alert alert--${hottest.celsius >= 80 ? "danger" : "warning"}`}>
          ⚠ Hottest zone: <strong>{hottest.zone}</strong> at{" "}
          <strong>{fmt(hottest.celsius, 1)} °C</strong>
        </div>
      )}

      <div className="thermal-grid">
        {temperatures.map((entry) => (
          <ThermalCard key={entry.zone} zone={entry.zone} celsius={entry.celsius} />
        ))}
      </div>
    </>
  );
}

// ─── System Tab ───────────────────────────────────────────────────────────────

function SystemTab({ data }) {
  const cpu = data?.cpu ?? {};
  const os  = data?.os  ?? {};

  const elevatedTone = os.is_elevated ? "ok" : "warn";
  const elevatedText = os.is_elevated ? "Administrator" : "Standard user";

  return (
    <>
      {/* ── OS block ── */}
      <div className="section-head">
        <h2>Operating System</h2>
        <span className="section-badge">/api/sensors · os</span>
      </div>

      <div className="info-card">
        <div className="info-row">
          <InfoItem label="OS Name"     value={os.name} />
          <InfoItem label="Version"     value={os.display_version} />
          <InfoItem label="Build"       value={os.build} mono />
          <InfoItem label="Architecture" value={os.architecture} />
        </div>
        <div className="info-row info-row--last">
          <InfoItem label="Hostname"   value={os.hostname} mono />
          <InfoItem label="Privileges" value={elevatedText} tone={elevatedTone} />
        </div>
      </div>

      <div className="section-spacer" />

      {/* ── CPU block ── */}
      <div className="section-head">
        <h2>Processor</h2>
        <span className="section-badge">/api/sensors · cpu</span>
      </div>

      <div className="info-card">
        <div className="info-brand">
          <span className="info-brand__name">{cpu.brand ?? "–"}</span>
          <span className="info-brand__sub">
            {[cpu.vendor, cpu.architecture].filter(Boolean).join(" · ")}
          </span>
        </div>

        <div className="info-grid">
          <InfoItem label="Physical Cores" value={cpu.physical_cores} />
          <InfoItem label="Logical Cores"  value={cpu.logical_cores} />
          <InfoItem label="Base Clock"
                    value={cpu.base_freq_mhz ? `${cpu.base_freq_mhz} MHz` : "–"} />
          <InfoItem label="Max Boost"
                    value={cpu.max_freq_mhz  ? `${cpu.max_freq_mhz} MHz`  : "–"} />
          <InfoItem label="L2 Cache / core" value={fmtCache(cpu.l2_cache_kb)} />
          <InfoItem label="L3 Cache (total)" value={fmtCache(cpu.l3_cache_kb)} />
          <InfoItem label="Architecture"    value={cpu.architecture} />
          <InfoItem label="Vendor ID"       value={cpu.vendor} mono />
        </div>
      </div>
    </>
  );
}

// ─── Root App ─────────────────────────────────────────────────────────────────

export default function App() {
  const [theme, setTheme] = useState(
    () => localStorage.getItem("theme") ?? "dark"
  );
  const [activeTab, setActiveTab] = useState("overview");
  // Default OFF — demo mode is opt-in for presentations
  const [mockMode, setMockMode] = useState(
    () => localStorage.getItem("mockMode") === "true"
  );

  useEffect(() => {
    document.documentElement.setAttribute("data-theme", theme);
    localStorage.setItem("theme", theme);
  }, [theme]);

  const toggleTheme = () => setTheme((t) => (t === "dark" ? "light" : "dark"));
  const toggleMock  = () =>
    setMockMode((m) => {
      const next = !m;
      localStorage.setItem("mockMode", String(next));
      return next;
    });

  const sensors = useQuery(sensorsQueryOptions());
  const disks   = useQuery(disksQueryOptions());
  const ping    = useQuery(pingQueryOptions());

  const sensorData = mockMode ? MOCK_SENSORS : sensors.data;
  const diskData   = mockMode ? MOCK_DISKS   : disks.data;
  const lastUpdate = mockMode ? null         : sensors.dataUpdatedAt;
  const isFetching = mockMode ? false        : sensors.isFetching;

  const hasData    = Boolean(sensors.data);
  const liveStatus = getStatus({
    isLoading: sensors.isLoading,
    isFetching: sensors.isFetching,
    isError: sensors.isError,
    hasData,
    error: sensors.error,
  });

  return (
    <>
      <Navbar
        theme={theme}
        onThemeToggle={toggleTheme}
        activeTab={activeTab}
        onTabChange={setActiveTab}
        mockMode={mockMode}
        onMockToggle={toggleMock}
        ping={ping}
      />

      {mockMode && (
        <div className="demo-banner">
          <span className="demo-banner__label">DEMO</span>
          <span>Simulated telemetry — for presentation purposes only</span>
          <button className="demo-banner__dismiss" onClick={toggleMock} type="button">
            Exit demo
          </button>
        </div>
      )}

      <main className="app-shell">
        {!mockMode && liveStatus.detail && (
          <div className={`alert alert--${liveStatus.tone === "danger" ? "danger" : "warning"}`}>
            {liveStatus.detail}
          </div>
        )}

        {activeTab === "overview" && <OverviewTab data={sensorData} />}
        {activeTab === "disks"    && (
          <DisksTab
            data={diskData}
            isLoading={!mockMode && disks.isLoading}
            isError={!mockMode && disks.isError}
          />
        )}
        {activeTab === "thermal"  && (
          <ThermalTab temperatures={sensorData?.temperatures ?? []} />
        )}
        {activeTab === "system"   && <SystemTab data={sensorData} />}
      </main>

      <Footer lastUpdate={lastUpdate} isFetching={isFetching} mockMode={mockMode} />
    </>
  );
}
