"use cache";
import { fetchSensors } from "@/api/Sensors";
import { connection } from "next/server";
import { Suspense } from "react";
import { ClientComponent } from "./components/Client";
import { getCurrentDayUnixRange } from "./utils/Time";
import { SensorType } from "@/types/Sensor";
import { notFound } from "next/navigation";
import { getMeasurementsFromSensor } from "@/api/Measurements";
import { MeasurementType } from "@/types/Measurement";

export default async function Home() {
  // HAETAAN LISTA SENSOREISTA
  const sensors: SensorType[] = await fetchSensors();
  // SENSOREITA EI LÖYTYNYT > OHJATAAN 404-SIVULLE
  if (!sensors || sensors.length == 0) return notFound();
  // HAETAAN ENSIMMÄISEN (satunnainen) SENSORIN MITTAUKSET NYKYISELTÄ PÄIVÄLTÄ
  const currentDayUnixRange = getCurrentDayUnixRange();
  const initialMeasurements: MeasurementType[] =
    await getMeasurementsFromSensor({
      ...currentDayUnixRange,
      sensorId: sensors[1]?.sensorId,
    });
  return (
    <div className="p-2">
      <Suspense fallback={<p>Sivua ladataan yhä.</p>}>
        <ClientComponent
          sensors={sensors}
          initialSelectedSensor={sensors[1]?.sensorId}
          initialMeasurements={initialMeasurements}
        />
      </Suspense>
    </div>
  );
}
