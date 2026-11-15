"use client";
import { MeasurementType } from "../types/Measurement";
import {
  Chart as ChartJS,
  ArcElement,
  Tooltip,
  Legend,
  CategoryScale,
  LinearScale,
  LineElement,
  PointElement,
} from "chart.js";
import { Line } from "react-chartjs-2";
import { convertTime } from "../utils/Date";

ChartJS.register(
  ArcElement,
  Tooltip,
  Legend,
  CategoryScale,
  LinearScale,
  LineElement,
  PointElement,
);

export const ChartComponent = ({
  measurements,
  chartType,
}: {
  measurements: MeasurementType[];
  chartType: "temperature" | "humidity";
}) => {
  // NÄYTTÄÄ VAIN ANNETUN DATAN!!! TILANHALLINTA SENSORCONTROLLER
  const isTemperatureChart = chartType === "temperature";
  const chartData = {
    labels: measurements.map((measurement) =>
      convertTime(measurement.timeStamp),
    ),
    datasets: [
      {
        label: isTemperatureChart ? "Lämpötila" : "Kosteus",
        data: measurements.map((measurement) => measurement[chartType]),
        fill: false,
        borderColor: isTemperatureChart
          ? "oklch(66.6% 0.179 58.318)"
          : "oklch(82.8% 0.189 84.429)",
        backgroundColor: isTemperatureChart
          ? "oklch(66.6% 0.179 58.318)"
          : "oklch(82.8% 0.189 84.429)",
        tension: 0.1,
        pointRadius: 0,
        pointHoverRadius: 6,
        pointHitRadius: 25,
      },
    ],
  };
  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    scales: {
      x: {
        ticks: {
          maxTicksLimit: 5,
          font: {
            size: 10,
          },
        },
      },
    },
  };
  return (
    <div className="w-full h-100">
      <Line options={chartOptions} data={chartData} />
    </div>
  );
};
