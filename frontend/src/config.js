// Crow backend address — shown in the navbar and used for health pings.
// Override at build time via the VITE_CROW_URL env variable.
// In dev, Vite proxies /api → this address (see vite.config.js).
// In production the C++ server at this address serves everything directly.
export const CROW_SERVER_URL = import.meta.env.VITE_CROW_URL ?? "localhost:18080";
