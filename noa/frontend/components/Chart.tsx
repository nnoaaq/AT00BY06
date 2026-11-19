"use client";
import {
  Chart as ChartJS,
  ArcElement,
  Tooltip,
  Legend,
  CategoryScale,
  TimeScale,
  LinearScale,
  LineElement,
  PointElement,
} from "chart.js";
import { Line } from "react-chartjs-2";
import "chartjs-adapter-date-fns";
import { fi } from "date-fns/locale";
import { MeasurementType } from "../types/MeasurementCache";

ChartJS.register(
  ArcElement,
  Tooltip,
  Legend,
  CategoryScale,
  TimeScale,
  LinearScale,
  LineElement,
  PointElement,
);

export const ChartComponent2 = ({
  measurements,
  chartType,
}: {
  measurements: MeasurementType[];
  chartType: "temperature" | "humidity";
}) => {
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
    datasets: [
      {
        label: isTemperatureChart ? "Lämpötila" : "Kosteus",
        data: measurements.map((measurement) => ({
          x: measurement.timeStamp * 1000, // poista * 1000 jos aikaleima on jo millisekunneissa
          y: measurement[chartType],
        })),
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

  const timestamps = measurements.map((m) => m.timeStamp * 1000);
  const minTime = timestamps.length ? Math.min(...timestamps) : 0;
  const maxTime = timestamps.length ? Math.max(...timestamps) : 0;
  const spanHours = (maxTime - minTime) / (1000 * 60 * 60);

  const timeUnit: "minute" | "hour" | "day" =
    spanHours <= 12 ? "minute" : spanHours <= 48 ? "hour" : "day";

  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: {
        position: "bottom" as const,
      },
      tooltip: {
        callbacks: {
          title: (items: { parsed: { x: number | null } }[]) => {
            const x = items[0]?.parsed?.x;
            if (!x) return "";
            return new Date(x).toLocaleString("fi-FI", {
              day: "2-digit",
              month: "2-digit",
              year: "numeric",
              hour: "2-digit",
              minute: "2-digit",
            });
          },
        },
      },
    },
    scales: {
      x: {
        type: "time" as const,
        adapters: {
          date: {
            locale: fi,
          },
        },
        time: {
          unit: timeUnit,
          tooltipFormat: "dd.MM.yyyy HH:mm",
          displayFormats: {
            minute: "HH:mm",
            hour: spanHours > 24 ? "dd.MM. HH:mm" : "HH:mm",
            day: "dd.MM.",
          },
        },
        ticks: {
          autoSkip: true,
          maxTicksLimit: spanHours > 48 ? 7 : 6,
        },
      },
      y: {
        ticks: {
          callback: (value: string | number) =>
            `${value}${isTemperatureChart ? " °C" : " %"}`,
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