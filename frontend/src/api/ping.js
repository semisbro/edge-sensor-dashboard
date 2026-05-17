export const PING_QUERY_KEY = ["server-ping"];
export const PING_INTERVAL_MS = 5_000;

export async function pingServer() {
  const t0 = performance.now();
  const res = await fetch("/api/meta", {
    signal: AbortSignal.timeout(4_000),
  });
  const latencyMs = Math.round(performance.now() - t0);

  if (!res.ok) throw new Error(`HTTP ${res.status}`);

  return { online: true, latencyMs, statusCode: res.status };
}

export function pingQueryOptions() {
  return {
    queryKey: PING_QUERY_KEY,
    queryFn: pingServer,
    refetchInterval: PING_INTERVAL_MS,
    refetchIntervalInBackground: true,
    retry: 0,
  };
}
