export const SENSOR_QUERY_KEY = ["sensors"];
export const SENSOR_POLL_INTERVAL_MS = 2_000;

export async function fetchSensorSnapshot() {
  const response = await fetch("/api/sensors");

  if (!response.ok) {
    throw new Error(`Backend returned ${response.status}`);
  }

  return response.json();
}

export function sensorsQueryOptions() {
  return {
    queryKey: SENSOR_QUERY_KEY,
    queryFn: fetchSensorSnapshot,
    refetchInterval: SENSOR_POLL_INTERVAL_MS,
    refetchIntervalInBackground: true,
  };
}
