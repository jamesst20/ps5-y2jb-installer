<script>
	import { Select as SelectPrimitive } from "bits-ui";
	import { cn } from "$lib/utils.js";
	let {
		ref = $bindable(null),
		class: className,
		children,
		...restProps
	} = $props();
</script>

<SelectPrimitive.GroupHeading
	bind:ref
	data-slot="select-group-heading"
	class={cn("text-muted-foreground px-1.5 py-1 text-xs", className)}
	{...restProps}
>
	{@render children?.()}
</SelectPrimitive.GroupHeading>