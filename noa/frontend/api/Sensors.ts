const { databaseUrl } = process.env;

// FUNKTIO JOKA HAKEE SENSORIT TIETOKANNASTA
// PALAUTTAA AINA TAULUKON
export const fetchSensors = async () => {
  try {
    if (!databaseUrl) throw new Error("Ympäristömuuttujat...");
    const output = await fetch(`${databaseUrl}/sensors`, {
      cache: "no-cache",
    });
    if (output.ok) return await output.json();
    throw new Error(`output virhe: ${output.status} - ${output.statusText}`);
  } catch (error) {
    console.log("fetchSensors() virhe:", error);
    return [];
  }
};
