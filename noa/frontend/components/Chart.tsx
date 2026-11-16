"use client";
import { MeasurementType } from "@/types/Measurement";
import { convertTime } from "@/utils/Date";
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
  const convertTailwindColor = (variableName: string) => {
    if (typeof window === "undefined") return "";
    return getComputedStyle(document.documentElement)
      .getPropertyValue(variableName)
      .trim();
  };
  const chartColor = isTemperatureChart
    ? convertTailwindColor("--color-temperature")
    : convertTailwindColor("--color-humidity");
  const chartData = {
    labels: measurements.map((measurement) =>
      convertTime(measurement.timeStamp),
    ),
    datasets: [
      {
        label: isTemperatureChart ? "Lämpötila" : "Kosteus",
        data: measurements.map((measurement) => measurement[chartType]),
        fill: false,
        borderColor: chartColor,
        backgroundColor: chartColor,
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
