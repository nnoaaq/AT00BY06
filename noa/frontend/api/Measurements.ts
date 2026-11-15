"use server";
const { databaseUrl } = process.env;

export const getMeasurementsFromSensor = async ({
  from,
  to,
  sensorId,
}: {
  from: number;
  to: number;
  sensorId: string;
}) => {
  if (!from || !to || !sensorId || !databaseUrl) return [];
  try {
    const output = await fetch(
      `${databaseUrl}/measurements/range?sensorId=${sensorId}&from=${from}&to=${to}`,
      { cache: "no-cache" },
    );

    if (output.ok) return await output.json();
    return [];
  } catch (error) {
    return [];
  }
};
