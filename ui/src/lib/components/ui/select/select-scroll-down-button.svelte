<script>
	import { Select as SelectPrimitive } from "bits-ui";
	import ChevronDownIcon from '@lucide/svelte/icons/chevron-down';
	import { cn } from "$lib/utils.js";

	let {
		ref = $bindable(null),
		class: className,
		...restProps
	} = $props();
</script>

<SelectPrimitive.ScrollDownButton
	bind:ref
	data-slot="select-scroll-down-button"
	class={cn("bg-popover z-10 flex cursor-default items-center justify-center py-1 [&_svg:not([class*='size-'])]:size-4", className)}
	{...restProps}
>
	<ChevronDownIcon  />
</SelectPrimitive.ScrollDownButton>