import { For, Show, createSignal } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import { onButtonPress } from "@pocketjs/framework/lifecycle";

const videos = [
  { title: "Naruto AMV - Best Moments", channel: "Anime World", views: "1.2M views", thumb: "NARUTO" },
  { title: "PSP Homebrew Showcase 2026", channel: "PSP Nexus", views: "248K views", thumb: "PSP" },
  { title: "Minecraft Speedrun", channel: "Retro Gamer", views: "913K views", thumb: "MINE" },
  { title: "Best Anime Openings", channel: "Otaku Hub", views: "3.4M views", thumb: "ANIME" },
];

const menu = ["Home", "Trending", "Shorts", "Library", "Settings"];

export default function App() {
  const [selected, setSelected] = createSignal(0);
  const [menuOpen, setMenuOpen] = createSignal(false);
  const [videoOpen, setVideoOpen] = createSignal(false);
  const [query, setQuery] = createSignal("");
  const [searchMode, setSearchMode] = createSignal(false);

  onButtonPress((button: string) => {
    if (button === "UP") setSelected((v) => Math.max(0, v - 1));
    if (button === "DOWN") setSelected((v) => Math.min(videos.length - 1, v + 1));
    if (button === "LEFT") setMenuOpen(true);
    if (button === "RIGHT") setMenuOpen(false);
    if (button === "CIRCLE") setVideoOpen(true);
    if (button === "CROSS") setVideoOpen(false);
    if (button === "TRIANGLE") setSearchMode(true);
    if (button === "L") setMenuOpen(true);
  });

  return (
    <View class="w-full h-full bg-slate-950 text-white flex-col">
      <View class="h-12 w-full flex-row items-center px-3 gap-3 bg-slate-900">
        <View class="w-9 h-7 rounded-lg bg-red-600 items-center justify-center">
          <Text class="font-bold text-white">▶</Text>
        </View>
        <Text class="font-bold text-xl text-white">YouTube</Text>
        <Show when={searchMode()}>
          <View class="flex-1 h-8 rounded-md bg-slate-800 border border-slate-600 px-3 justify-center">
            <Text class="text-slate-200">{query() || "Search…"}</Text>
          </View>
        </Show>
        <Show when={!searchMode()}>
          <View class="flex-1" />
        </Show>
        <Text class="text-slate-400">△ Search</Text>
      </View>

      <View class="flex-1 flex-row">
        <Show when={menuOpen()}>
          <View class="w-32 h-full bg-slate-900 border-r border-slate-700 p-2 gap-2">
            <Text class="font-bold text-lg mb-2">Menu</Text>
            <For each={menu}>
              {(item, i) => (
                <View class={`h-9 rounded-md px-2 justify-center ${i() === 0 ? "bg-red-600" : "bg-slate-800"}`}>
                  <Text class="text-white">{item}</Text>
                </View>
              )}
            </For>
          </View>
        </Show>

        <Show when={!videoOpen()} fallback={
          <View class="flex-1 p-4 gap-3">
            <Text class="text-2xl font-bold">{videos[selected()].title}</Text>
            <View class="w-full h-36 bg-black border border-slate-700 items-center justify-center">
              <Text class="text-slate-400 text-lg">VIDEO PLAYER</Text>
            </View>
            <Text class="font-bold text-lg">{videos[selected()].channel}</Text>
            <Text class="text-slate-400">{videos[selected()].views}</Text>
            <Text class="text-slate-300">Description and playback controls will be connected to the Pocket YouTube backend next.</Text>
            <Text class="text-slate-500">○ Back</Text>
          </View>
        }>
          <View class="flex-1 p-3 gap-3">
            <Text class="text-slate-400">Home</Text>
            <Text class="text-2xl font-bold">Recommended</Text>
            <For each={videos}>
              {(video, i) => (
                <View class={`w-full h-42 rounded-lg flex-row p-2 gap-3 ${selected() === i() ? "border-2 border-red-500 bg-slate-800" : "border border-slate-700 bg-slate-900"}`}>
                  <View class="w-40 h-24 rounded-md bg-slate-700 items-center justify-center">
                    <Text class="font-bold text-slate-200">{video.thumb}</Text>
                  </View>
                  <View class="flex-1 justify-center gap-1">
                    <Text class="font-bold text-white text-lg">{video.title}</Text>
                    <Text class="text-slate-300">{video.channel}</Text>
                    <Text class="text-slate-500">{video.views}</Text>
                  </View>
                </View>
              )}
            </For>
          </View>
        </Show>
      </View>

      <View class="h-7 w-full bg-slate-900 flex-row items-center justify-end px-3 gap-4">
        <Text class="text-slate-500">D-pad Navigate</Text>
        <Text class="text-slate-500">○ Open</Text>
        <Text class="text-slate-500">L Menu</Text>
      </View>
    </View>
  );
}
